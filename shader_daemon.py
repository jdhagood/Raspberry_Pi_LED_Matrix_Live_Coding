#!/usr/bin/env python3
import os
import time
import json
import signal
import traceback

import numpy as np
import moderngl

from rgbmatrix import RGBMatrix, RGBMatrixOptions

# -------------------------------------------------------------------
# Config
# -------------------------------------------------------------------

# LED wall layout: 3 parallel chains of 4 64x64 panels = 256x192
MATRIX_WIDTH = 256
MATRIX_HEIGHT = 192

# Files written by Flask
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
SHADER_PATH = os.path.join(BASE_DIR, "runtime_shader.glsl")
FFT_PATH = os.path.join(BASE_DIR, "runtime_fft.json")

# How many FFT bins we expect (as sent by the browser)
NUM_FFT_BINS = 32

# Target FPS
TARGET_FPS = 30.0
FRAME_DT = 1.0 / TARGET_FPS

# -------------------------------------------------------------------
# GLSL wrapper
# We take the user fragment body and wrap it with a header that exposes:
#   uniform vec2  u_resolution;
#   uniform float u_time;
#   uniform vec2  u_mouse;      // currently (0,0)
#   uniform float u_fft[32];    // audio spectrum
#   in vec2 v_uv;               // 0..1
# -------------------------------------------------------------------

VERTEX_SHADER_SRC = """
#version 330

in vec2 in_position;
out vec2 v_uv;

void main() {
    v_uv = in_position * 0.5 + 0.5;
    gl_Position = vec4(in_position, 0.0, 1.0);
}
"""

FRAGMENT_HEADER = f"""
#version 330

uniform vec2  u_resolution;
uniform float u_time;
uniform vec2  u_mouse;
uniform float u_fft[{NUM_FFT_BINS}];

in vec2 v_uv;
out vec4 fragColor;

// NOTE: User shader must define 'void main()' using v_uv, u_time, u_resolution, u_mouse, u_fft.
"""

# -------------------------------------------------------------------
# Globals
# -------------------------------------------------------------------

running = True


def handle_signal(signum, frame):
    global running
    print(f"Received signal {signum}, shutting down...")
    running = False


signal.signal(signal.SIGINT, handle_signal)
signal.signal(signal.SIGTERM, handle_signal)


# -------------------------------------------------------------------
# Utility: load shader source & FFT
# -------------------------------------------------------------------

def load_user_shader_source():
    """Return (source_string, mtime) or (None, None) if file missing."""
    if not os.path.exists(SHADER_PATH):
        return None, None
    try:
        mtime = os.path.getmtime(SHADER_PATH)
        with open(SHADER_PATH, "r", encoding="utf-8") as f:
            src = f.read()
        return src, mtime
    except Exception as e:
        print("Error reading shader file:", e)
        return None, None


def load_fft():
    """Return a numpy array of shape (NUM_FFT_BINS,) with floats in [0,1]."""
    fft = np.zeros((NUM_FFT_BINS,), dtype="f4")
    if not os.path.exists(FFT_PATH):
        return fft
    try:
        with open(FFT_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
        arr = data.get("fft", [])
        if not isinstance(arr, list):
            return fft
        # clamp/resize
        arr = arr[:NUM_FFT_BINS]
        if len(arr) < NUM_FFT_BINS:
            arr = arr + [0.0] * (NUM_FFT_BINS - len(arr))
        fft[:] = np.array(arr, dtype="f4")
    except Exception as e:
        print("Error reading FFT file:", e)
    return fft


# -------------------------------------------------------------------
# ModernGL setup
# -------------------------------------------------------------------

def create_gl_context_and_pipeline():
    """
    Create a headless moderngl context and a basic pipeline:
      - FBO of size MATRIX_WIDTH x MATRIX_HEIGHT
      - Fullscreen quad VBO
      - Placeholder program; we recompile fragment shader when needed.
    """
    ctx = moderngl.create_context(standalone=True)
    ctx.viewport = (0, 0, MATRIX_WIDTH, MATRIX_HEIGHT)

    # Fullscreen quad (two triangles)
    quad_vertices = np.array(
        [
            -1.0, -1.0,
             1.0, -1.0,
            -1.0,  1.0,
            -1.0,  1.0,
             1.0, -1.0,
             1.0,  1.0,
        ],
        dtype="f4",
    )
    vbo = ctx.buffer(quad_vertices.tobytes())

    vao_content = [
        (vbo, "2f", "in_position"),
    ]

    # Dummy fragment shader to start with
    dummy_frag = FRAGMENT_HEADER + """
void main() {
    vec2 uv = v_uv;
    float t = u_time;
    float v = 0.5 + 0.5 * sin(10.0 * (uv.x + uv.y) + t);
    fragColor = vec4(vec3(v), 1.0);
}
"""
    program = ctx.program(
        vertex_shader=VERTEX_SHADER_SRC,
        fragment_shader=dummy_frag,
    )

    vao = ctx.vertex_array(program, vao_content)

    fbo = ctx.simple_framebuffer((MATRIX_WIDTH, MATRIX_HEIGHT), components=3)
    fbo.use()

    return ctx, vbo, vao, fbo, program


def compile_user_fragment(ctx, user_src):
    """
    Try to compile a new program with the user fragment body wrapped in FRAGMENT_HEADER.
    Returns a new moderngl.Program on success, or None on failure.
    """
    frag = FRAGMENT_HEADER + "\n" + user_src
    try:
        prog = ctx.program(
            vertex_shader=VERTEX_SHADER_SRC,
            fragment_shader=frag,
        )
        print("Shader compiled successfully.")
        return prog
    except Exception as e:
        print("Shader compile/link failed:")
        print(e)
        traceback.print_exc()
        return None


# -------------------------------------------------------------------
# LED matrix setup
# -------------------------------------------------------------------

def create_matrix():
    options = RGBMatrixOptions()
    options.rows = 64
    options.cols = 64
    options.chain_length = 4   # 4 panels per chain
    options.parallel = 3       # 3 chains
    options.hardware_mapping = "regular"
    options.brightness = 60
    options.pwm_bits = 8
    options.gpio_slowdown = 1
    options.show_refresh_rate = 0

    matrix = RGBMatrix(options=options)
    canvas = matrix.CreateFrameCanvas()

    if matrix.width != MATRIX_WIDTH or matrix.height != MATRIX_HEIGHT:
        print(
            f"Warning: matrix reports {matrix.width}x{matrix.height}, "
            f"expected {MATRIX_WIDTH}x{MATRIX_HEIGHT}"
        )
    return matrix, canvas


# -------------------------------------------------------------------
# Main render loop
# -------------------------------------------------------------------

def main():
    ctx, vbo, vao, fbo, program = create_gl_context_and_pipeline()
    matrix, canvas = create_matrix()

    start_time = time.time()

    # Track shader file changes
    last_shader_mtime = None
    last_good_program = program

    # For reading pixels: allocate a buffer once
    # moderngl simple_framebuffer.read() returns bytes row-major, bottom-to-top
    while running:
        frame_start = time.time()

        # 1) Check if shader file changed
        user_src, mtime = load_user_shader_source()
        if user_src is not None and mtime is not None:
            if last_shader_mtime is None or mtime > last_shader_mtime:
                print("Detected shader file change, recompiling...")
                new_prog = compile_user_fragment(ctx, user_src)
                if new_prog is not None:
                    # Rebuild VAO with new program
                    program = new_prog
                    vao = ctx.vertex_array(program, [(vbo, "2f", "in_position")])
                    last_good_program = program
                    last_shader_mtime = mtime
                else:
                    print("Keeping last good shader.")

        # Use last known good program
        prog = last_good_program

        # 2) Read FFT
        fft = load_fft()

        # 3) Render with moderngl
        fbo.use()
        ctx.clear(0.0, 0.0, 0.0, 1.0)

        prog["u_resolution"].value = (float(MATRIX_WIDTH), float(MATRIX_HEIGHT))
        t = time.time() - start_time
        prog["u_time"].value = float(t)
        prog["u_mouse"].value = (0.0, 0.0)  # can be wired later if you stream mouse

        # Upload FFT as uniform array
        try:
            # For moderngl, we can set array uniform via tuple
            prog["u_fft"].value = tuple(float(x) for x in fft)
        except KeyError:
            # Shader may not declare u_fft; ignore
            pass

        vao.render()

        # 4) Read pixels and push to LED matrix
        # returns bytes in RGB, bottom row first
        data = fbo.read(components=3, alignment=1)
        img = np.frombuffer(data, dtype=np.uint8).reshape((MATRIX_HEIGHT, MATRIX_WIDTH, 3))

        # Flip vertically to make (0,0) top-left for the matrix
        img = np.flipud(img)

        # Push to matrix
        # You can optimize this with Cython or row-wise loops later
        for y in range(MATRIX_HEIGHT):
            row = img[y]
            for x in range(MATRIX_WIDTH):
                r, g, b = row[x]
                canvas.SetPixel(x, y, int(r), int(g), int(b))

        canvas = matrix.SwapOnVSync(canvas)

        # 5) Frame pacing
        frame_time = time.time() - frame_start
        sleep_time = FRAME_DT - frame_time
        if sleep_time > 0:
            time.sleep(sleep_time)

    print("Clearing matrix and exiting.")
    matrix.Clear()


if __name__ == "__main__":
    main()
