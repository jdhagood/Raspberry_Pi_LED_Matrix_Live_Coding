from flask import Flask, render_template, request, jsonify
import socket

app = Flask(__name__, static_folder="static", template_folder="templates")

LED_HOST = "127.0.0.1"
LED_PORT = 9999

sock = None  # TCP socket


def get_socket():
  global sock
  if sock is not None:
    return sock
  s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  s.connect((LED_HOST, LED_PORT))
  sock = s
  return sock


@app.route("/")
def index():
  return render_template("index.html")


@app.post("/frame")
def frame():
  payload = request.get_json(force=True)
  w = int(payload["w"])
  h = int(payload["h"])
  data = payload["data"]

  if w != 256 or h != 192:
    return jsonify({"status": "error", "reason": "expected 256x192"}), 400
  if len(data) != w * h * 3:
    return jsonify({"status": "error", "reason": "bad data length"}), 400

  buf = bytes(data)
  try:
    s = get_socket()
    s.sendall(buf)
  except OSError as e:
    global sock
    if sock is not None:
      try:
        sock.close()
      except OSError:
        pass
    sock = None
    return jsonify({"status": "error", "reason": f"matrix connection: {e}"}), 500

  return jsonify({"status": "ok"})


if __name__ == "__main__":
  app.run(host="0.0.0.0", port=5000, debug=True)
