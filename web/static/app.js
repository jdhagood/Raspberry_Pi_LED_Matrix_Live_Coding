// Simple WebGL fragment shader live-coding environment.
// Features:
// - CodeMirror with GLSL highlighting
// - Ctrl+S / Cmd+S to compile
// - Fixed-size preview canvas
// - Dark/light mode toggle
// - Preset shader dropdown

/* PRESET SHADERS */

const PRESETS = {
  default: `
// Rings (default)
void main() {
    vec2 uv = v_uv;
    vec2 p = (uv - 0.5) * 2.0;
    float t = u_time * 0.5;

    float d = length(p);
    float ring = 0.5 + 0.5 * cos(10.0 * d - t * 6.28318);

    vec3 col = vec3(0.0);
    col.r = ring;
    col.g = 0.5 + 0.5 * sin(t + p.x * 4.0);
    col.b = 0.5 + 0.5 * sin(t + p.y * 4.0);

    gl_FragColor = vec4(col, 1.0);
}
`.trim(),

  plasma: `
// Classic plasma
float hash(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

void main() {
    vec2 uv = (v_uv - 0.5) * 2.0;
    float t = u_time * 0.3;

    float v = 0.0;
    v += sin(uv.x * 3.0 + t);
    v += sin(uv.y * 4.0 - t * 1.3);
    v += sin((uv.x + uv.y) * 5.0 + t * 0.7);
    v /= 3.0;

    vec3 col = 0.5 + 0.5 * cos(6.28318 * (v + vec3(0.0, 0.33, 0.67)));

    gl_FragColor = vec4(col, 1.0);
}
`.trim(),

  gradient: `
// Simple animated gradient
void main() {
    vec2 uv = v_uv;
    float t = u_time * 0.2;

    vec3 col;
    col.r = uv.x;
    col.g = uv.y;
    col.b = 0.5 + 0.5 * sin(t + uv.x * 6.28318);

    gl_FragColor = vec4(col, 1.0);
}
`.trim(),

  mouse_ripple: `
// Mouse ripple
void main() {
    vec2 uv = v_uv;
    vec2 mouse = u_mouse / u_resolution;
    vec2 p = uv - mouse;

    float d = length(p);
    float t = u_time;

    float wave = 0.5 + 0.5 * cos(20.0 * d - t * 6.28318);
    float falloff = exp(-10.0 * d);

    float v = wave * falloff;

    vec3 base = vec3(0.05, 0.08, 0.12);
    vec3 ring = vec3(0.1, 0.7, 1.0);

    vec3 col = base + v * ring;

    gl_FragColor = vec4(col, 1.0);
}
`.trim()
};

const DEFAULT_FRAGMENT_SOURCE = PRESETS.default;

/* GLOBALS */

let gl;
let canvas;
let errorLog;
let presetSelect;
let themeToggle;
let cmEditor = null;

let quadBuffer;
let vertexShader;
let currentProgram = null;
let uniformLocations = {};

let startTime = performance.now();
const mouse = { x: 0, y: 0 };

const FRAGMENT_HEADER = `
precision highp float;

uniform vec2  u_resolution;
uniform float u_time;
uniform vec2  u_mouse;

varying vec2  v_uv;
`;

document.addEventListener("DOMContentLoaded", () => {
  canvas = document.getElementById("shader-canvas");
  errorLog = document.getElementById("error-log");
  presetSelect = document.getElementById("shader-preset");
  themeToggle = document.getElementById("theme-toggle");

  initTheme();
  initEditor();
  initGL();
  initEvents();

  // Compile once at startup
  compileAndUseShader(cmEditor.getValue());
  requestAnimationFrame(renderLoop);
});



let lastSentTime = 0;
const SEND_INTERVAL_MS = 50; // 20 fps cap


/* THEME HANDLING */

function initTheme() {
  const root = document.documentElement;
  const saved = localStorage.getItem("shader_theme") || "light";
  root.setAttribute("data-theme", saved);

  if (saved === "dark") {
    themeToggle.textContent = "Light mode";
  } else {
    themeToggle.textContent = "Dark mode";
  }
}

/* EDITOR / CODEMIRROR */

function initEditor() {
  const textarea = document.getElementById("shader-editor");
  textarea.value = DEFAULT_FRAGMENT_SOURCE;

  cmEditor = CodeMirror.fromTextArea(textarea, {
    mode: "x-shader/x-fragment", // GLSL
    lineNumbers: true,
    indentUnit: 2,
    tabSize: 2,
    indentWithTabs: false,
    smartIndent: true,
    matchBrackets: true,
    autofocus: true,
    theme: document.documentElement.getAttribute("data-theme") === "dark"
      ? "material-darker"
      : "default",
    extraKeys: {
      "Ctrl-S": function (cm) {
        compileAndUseShader(cm.getValue());
      },
      "Cmd-S": function (cm) {
        compileAndUseShader(cm.getValue());
      }
    }
  });
}

/* GL INIT */

function initGL() {
  resizeCanvasToElement();

  gl = canvas.getContext("webgl2", { antialias: true }) ||
       canvas.getContext("webgl", { antialias: true });

  if (!gl) {
    logError("WebGL is not supported by this browser.");
    return;
  }

  // Fullscreen quad
  quadBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, quadBuffer);
  const quadVertices = new Float32Array([
    -1.0, -1.0,
     1.0, -1.0,
    -1.0,  1.0,
     1.0,  1.0,
  ]);
  gl.bufferData(gl.ARRAY_BUFFER, quadVertices, gl.STATIC_DRAW);

  // Vertex shader
  const vsSource = `
    attribute vec2 a_position;
    varying vec2 v_uv;
    void main() {
      v_uv = a_position * 0.5 + 0.5;
      gl_Position = vec4(a_position, 0.0, 1.0);
    }
  `;
  vertexShader = compileShader(gl.VERTEX_SHADER, vsSource);
}

/* EVENTS */

function initEvents() {
  window.addEventListener("resize", () => {
    resizeCanvasToElement();
    if (gl && canvas) {
      gl.viewport(0, 0, canvas.width, canvas.height);
    }
  });

  canvas.addEventListener("mousemove", (e) => {
    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = rect.bottom - e.clientY; // invert Y so 0 at bottom
    mouse.x = x * window.devicePixelRatio;
    mouse.y = y * window.devicePixelRatio;
  });

  presetSelect.addEventListener("change", () => {
    const key = presetSelect.value;
    const src = PRESETS[key] || DEFAULT_FRAGMENT_SOURCE;
    cmEditor.setValue(src);
    compileAndUseShader(cmEditor.getValue());
  });

  themeToggle.addEventListener("click", () => {
    const root = document.documentElement;
    const current = root.getAttribute("data-theme") || "light";
    const next = current === "light" ? "dark" : "light";
    root.setAttribute("data-theme", next);
    localStorage.setItem("shader_theme", next);

    // Update CM theme to match
    cmEditor.setOption("theme", next === "dark" ? "material-darker" : "default");
    themeToggle.textContent = next === "dark" ? "Light mode" : "Dark mode";
  });
}

/* CANVAS SIZE */

function resizeCanvasToElement() {
  if (!canvas) return;
  const w = 256;
  const h = 192;
  canvas.width = w;
  canvas.height = h;
}


/* COMPILATION */

function compileAndUseShader(userSource) {
  clearError();

  if (!gl || !vertexShader) {
    logError("WebGL not initialized.");
    return;
  }

  const fullSource = FRAGMENT_HEADER + "\n\n" + userSource;
  const fragmentShader = compileShader(gl.FRAGMENT_SHADER, fullSource);
  if (!fragmentShader) {
    return; // compileShader already logged errors
  }

  const program = gl.createProgram();
  gl.attachShader(program, vertexShader);
  gl.attachShader(program, fragmentShader);
  gl.linkProgram(program);

  const linked = gl.getProgramParameter(program, gl.LINK_STATUS);
  if (!linked) {
    const info = gl.getProgramInfoLog(program) || "Unknown link error";
    gl.deleteShader(fragmentShader);
    gl.deleteProgram(program);
    logError("Program link error:\n" + info);
    return;
  }

  if (currentProgram) {
    gl.deleteProgram(currentProgram);
  }
  currentProgram = program;

  uniformLocations = {
    u_resolution: gl.getUniformLocation(currentProgram, "u_resolution"),
    u_time: gl.getUniformLocation(currentProgram, "u_time"),
    u_mouse: gl.getUniformLocation(currentProgram, "u_mouse"),
  };

  logInfo("Shader compiled and linked successfully.");
}

function compileShader(type, src) {
  const shader = gl.createShader(type);
  gl.shaderSource(shader, src);
  gl.compileShader(shader);

  const ok = gl.getShaderParameter(shader, gl.COMPILE_STATUS);
  if (!ok) {
    const info = gl.getShaderInfoLog(shader) || "Unknown compile error";
    gl.deleteShader(shader);
    const typeName = type === gl.VERTEX_SHADER ? "VERTEX" : "FRAGMENT";
    logError(typeName + " shader compile error:\n" + info);
    return null;
  }
  return shader;
}

/* RENDER LOOP */

function renderLoop() {
  if (gl && currentProgram && canvas) {
    gl.viewport(0, 0, canvas.width, canvas.height);
    gl.clearColor(0.0, 0.0, 0.0, 1.0);
    gl.clear(gl.COLOR_BUFFER_BIT);

    gl.useProgram(currentProgram);

    const posLoc = gl.getAttribLocation(currentProgram, "a_position");
    gl.bindBuffer(gl.ARRAY_BUFFER, quadBuffer);
    gl.enableVertexAttribArray(posLoc);
    gl.vertexAttribPointer(posLoc, 2, gl.FLOAT, false, 0, 0);

    const t = (performance.now() - startTime) * 0.001;

    if (uniformLocations.u_resolution) {
      gl.uniform2f(uniformLocations.u_resolution, canvas.width, canvas.height);
    }
    if (uniformLocations.u_time) {
      gl.uniform1f(uniformLocations.u_time, t);
    }
    if (uniformLocations.u_mouse) {
      gl.uniform2f(uniformLocations.u_mouse, mouse.x, mouse.y);
    }

    gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);

// Send this frame to the Pi LED panel (throttled)
    sendFrameToServer();
  }

  requestAnimationFrame(renderLoop);
}

function sendFrameToServer() {
  if (!gl || !canvas) return;

  const now = performance.now();
  if (now - lastSentTime < SEND_INTERVAL_MS) {
    return; // throttle
  }
  lastSentTime = now;

  const w = canvas.width;
  const h = canvas.height;

  const pixels = new Uint8Array(w * h * 4);  // RGBA
  gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, pixels);

  // Convert to RGB only
  const rgb = new Array(w * h * 3);
  let src = 0;
  let dst = 0;
  for (let i = 0; i < w * h; i++) {
    rgb[dst++] = pixels[src++]; // R
    rgb[dst++] = pixels[src++]; // G
    rgb[dst++] = pixels[src++]; // B
    src++;                      // skip A
  }

  fetch("/frame", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ w, h, data: rgb })
  }).catch((err) => {
    // silently ignore; LED panel failing shouldn't break UI
    console.error("Failed to send frame:", err);
  });
}


/* LOGGING */

function logError(msg) {
  if (!errorLog) return;
  errorLog.textContent = msg;
  errorLog.classList.add("error-present");
}

function clearError() {
  if (!errorLog) return;
  errorLog.textContent = "";
  errorLog.classList.remove("error-present");
}

function logInfo(msg) {
  if (!errorLog) return;
  errorLog.textContent = msg;
  errorLog.classList.remove("error-present");
}
