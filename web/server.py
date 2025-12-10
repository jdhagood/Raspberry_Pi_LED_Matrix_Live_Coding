# web/server.py
from flask import Flask, render_template, request, jsonify
import os
import json
import threading

app = Flask(__name__, static_folder="static", template_folder="templates")

# Where we drop the current shader + FFT for the local renderer
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHADER_PATH = os.path.join(BASE_DIR, "runtime_shader.glsl")
FFT_PATH    = os.path.join(BASE_DIR, "runtime_fft.json")

# In-memory copies (optional, mainly for debugging)
current_shader_source = ""
current_fft = [0.0] * 32

lock = threading.Lock()


@app.route("/")
def index():
    # Renders your shader editor UI (index.html in templates/)
    return render_template("index.html")


@app.route("/shader", methods=["POST"])
def shader():
    """
    Receive GLSL fragment shader source from the browser.
    Body: JSON { "source": "<glsl string>" }
    """
    global current_shader_source
    data = request.get_json(force=True, silent=True)
    if not data or "source" not in data:
        return jsonify({"ok": False, "error": "Missing 'source'"}), 400

    src = data["source"]
    with lock:
        current_shader_source = src
        # Write to file so the local renderer (C++ or Python) can pick it up
        with open(SHADER_PATH, "w", encoding="utf-8") as f:
            f.write(src)

    return jsonify({"ok": True})


@app.route("/audio", methods=["POST"])
def audio():
    """
    Receive an FFT frame from the browser.
    Body: JSON { "fft": [f0, f1, ..., fN-1] }
    """
    global current_fft
    data = request.get_json(force=True, silent=True)
    if not data or "fft" not in data:
        return jsonify({"ok": False, "error": "Missing 'fft'"}), 400

    arr = data["fft"]
    if not isinstance(arr, list):
        return jsonify({"ok": False, "error": "fft must be list"}), 400

    # Optionally clamp size to 32 bins
    if len(arr) > 32:
        arr = arr[:32]
    elif len(arr) < 32:
        arr = arr + [0.0] * (32 - len(arr))

    with lock:
        current_fft = arr
        # Write to JSON for the local renderer to read
        with open(FFT_PATH, "w", encoding="utf-8") as f:
            json.dump({"fft": current_fft}, f)

    return jsonify({"ok": True})


if __name__ == "__main__":
    # Bind to all interfaces so your laptop can reach it
    app.run(host="0.0.0.0", port=5000, debug=False)
