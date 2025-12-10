#include "led-matrix.h"

#include <unistd.h>
#include <signal.h>
#include <cstdio>

using rgb_matrix::RGBMatrix;
using rgb_matrix::Canvas;

volatile bool interrupt_received = false;
static void InterruptHandler(int) {
  interrupt_received = true;
}

static void PanelColor(int idx, uint8_t &r, uint8_t &g, uint8_t &b) {
  switch (idx) {
    case 0:  r = 255; g = 0;   b = 0;   break; // red
    case 1:  r = 0;   g = 255; b = 0;   break; // green
    case 2:  r = 0;   g = 0;   b = 255; break; // blue
    case 3:  r = 255; g = 255; b = 0;   break; // yellow
    case 4:  r = 255; g = 0;   b = 255; break; // magenta
    case 5:  r = 0;   g = 255; b = 255; break; // cyan
    case 6:  r = 255; g = 128; b = 0;   break;
    case 7:  r = 128; g = 0;   b = 255; break;
    case 8:  r = 128; g = 128; b = 128; break;
    case 9:  r = 255; g = 255; b = 255; break;
    case 10: r = 128; g = 255; b = 0;   break;
    case 11: r = 0;   g = 128; b = 255; break;
    default: r = g = b = 0;             break;
  }
}

static void DrawPanels(Canvas *canvas) {
  const int panel_w = 64;
  const int panel_h = 64;
  const int grid_cols = 4;
  const int grid_rows = 3;

  const int width  = canvas->width();   // should be 256
  const int height = canvas->height();  // should be 192

  std::fprintf(stderr, "Canvas size: %dx%d\n", width, height);

  canvas->Fill(0, 0, 0);

  for (int row = 0; row < grid_rows; ++row) {
    for (int col = 0; col < grid_cols; ++col) {
      int idx = row * grid_cols + col; // 0..11
      uint8_t r, g, b;
      PanelColor(2, r, g, b);

      int x0 = col * panel_w;
      int y0 = row * panel_h;

      for (int y = 0; y < panel_h; ++y) {
        for (int x = 0; x < panel_w; ++x) {
          canvas->SetPixel(x0 + x, y0 + y, r, g, b);
        }
      }
    }
  }

  // Keep image displayed
  while (!interrupt_received) {
    usleep(100 * 1000);
  }
}

int main(int argc, char *argv[]) {
  RGBMatrix::Options defaults;
  defaults.hardware_mapping = "regular";
  defaults.rows         = 64;
  defaults.cols         = 64;
  defaults.chain_length = 4;
  defaults.parallel     = 3;
  defaults.show_refresh_rate = true;

  Canvas *canvas = RGBMatrix::CreateFromFlags(&argc, &argv, &defaults);
  if (canvas == nullptr)
    return 1;

  signal(SIGTERM, InterruptHandler);
  signal(SIGINT,  InterruptHandler);

  DrawPanels(canvas);

  canvas->Clear();
  delete canvas;
  return 0;
}
