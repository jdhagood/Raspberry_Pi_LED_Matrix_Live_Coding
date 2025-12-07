from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route("/")
def index():
    return "Live LED Matrix server up"

@app.route("/shader", methods=["POST"])
def upload_shader():
    data = request.get_json(force=True)
    shader_code = data.get("shader", "")
    # TODO: compile shader, update running renderer, push frames to matrix
    return jsonify({"status": "ok", "length": len(shader_code)})

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080, debug=True)
