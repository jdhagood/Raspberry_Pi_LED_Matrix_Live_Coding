#include "led-matrix.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

using rgb_matrix::RGBMatrix;
using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;

static const int LOGICAL_WIDTH  = 256;
static const int LOGICAL_HEIGHT = 192;

static const int PANEL_W   = 64;
static const int PANEL_H   = 64;
static const int GRID_COLS = 4;
static const int GRID_ROWS = 3;

static const int PORT = 9999;

volatile bool interrupt_received = false;
static void InterruptHandler(int) {
  interrupt_received = true;
}

static bool ReadNBytes(int fd, uint8_t *buf, size_t n) {
  size_t total = 0;
  while (total < n) {
    ssize_t got = recv(fd, buf + total, n - total, 0);
    if (got <= 0) {
      return false;  // error or EOF
    }
    total += got;
  }
  return true;
}

int main(int argc, char *argv[]) {
  // Matrix config: 3 parallel chains of 4 panels
  RGBMatrix::Options defaults;
  defaults.hardware_mapping = "regular";
  defaults.rows         = PANEL_H;       // 64
  defaults.cols         = PANEL_W;       // 64
  defaults.chain_length = GRID_COLS;     // 4
  defaults.parallel     = GRID_ROWS;     // 3
  defaults.show_refresh_rate = true;

  RGBMatrix *matrix = RGBMatrix::CreateFromFlags(&argc, &argv, &defaults);
  if (matrix == nullptr) {
    std::fprintf(stderr, "Could not create RGBMatrix\n");
    return 1;
  }

  Canvas *canvas = matrix;
  if (canvas->width() != LOGICAL_WIDTH || canvas->height() != LOGICAL_HEIGHT) {
    std::fprintf(stderr, "Unexpected canvas size: %dx%d (expected %dx%d)\n",
                 canvas->width(), canvas->height(),
                 LOGICAL_WIDTH, LOGICAL_HEIGHT);
  }

  FrameCanvas *offscreen = matrix->CreateFrameCanvas();

  signal(SIGTERM, InterruptHandler);
  signal(SIGINT,  InterruptHandler);

  // TCP listen socket
  int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sock < 0) {
    perror("socket");
    delete matrix;
    return 1;
  }

  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // 127.0.0.1
  addr.sin_port        = htons(PORT);

  if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(listen_sock);
    delete matrix;
    return 1;
  }

  if (listen(listen_sock, 1) < 0) {
    perror("listen");
    close(listen_sock);
    delete matrix;
    return 1;
  }

  std::fprintf(stderr,
          "matrix_daemon listening on TCP 127.0.0.1:%d (logical %dx%d, panels %dx%d)\n",
          PORT, LOGICAL_WIDTH, LOGICAL_HEIGHT, GRID_COLS, GRID_ROWS);

  const size_t expected_size = LOGICAL_WIDTH * LOGICAL_HEIGHT * 3;
  static uint8_t buffer[LOGICAL_WIDTH * LOGICAL_HEIGHT * 3];

  while (!interrupt_received) {
    std::fprintf(stderr, "Waiting for connection from server.py...\n");
    int client = accept(listen_sock, nullptr, nullptr);
    if (client < 0) {
      if (interrupt_received) break;
      perror("accept");
      continue;
    }

    std::fprintf(stderr, "Client connected.\n");

    while (!interrupt_received) {
      if (!ReadNBytes(client, buffer, expected_size)) {
        std::fprintf(stderr, "Client disconnected.\n");
        close(client);
        break;
      }

      // buffer: row-major, origin at bottom-left (WebGL)
      size_t idx = 0;
      for (int y_buf = 0; y_buf < LOGICAL_HEIGHT; ++y_buf) {
        int Y = LOGICAL_HEIGHT - 1 - y_buf;  // flip vertically
        for (int X = 0; X < LOGICAL_WIDTH; ++X) {
          uint8_t r = buffer[idx++];
          uint8_t g = buffer[idx++];
          uint8_t b = buffer[idx++];
          offscreen->SetPixel(X, Y, r, g, b);
        }
      }

      offscreen = matrix->SwapOnVSync(offscreen);
    }
  }

  close(listen_sock);
  matrix->Clear();
  delete matrix;
  return 0;
}
