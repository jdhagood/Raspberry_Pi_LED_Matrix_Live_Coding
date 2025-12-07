// src/matrix_demo.cc
// Small example using hzeller/rpi-rgb-led-matrix with a single 64x64 panel.

#include "led-matrix.h"

#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <signal.h>

using rgb_matrix::RGBMatrix;
using rgb_matrix::Canvas;

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
  interrupt_received = true;
}

static void DrawOnCanvas(Canvas *canvas) {
  canvas->Fill(0, 0, 255);  // blue background

  const int center_x = canvas->width() / 2;
  const int center_y = canvas->height() / 2;
  const float radius_max = canvas->width() / 2.0f;
  const float angle_step = 1.0f / 360.0f;

  for (float a = 0.0f, r = 0.0f; r < radius_max; a += angle_step, r += angle_step) {
    if (interrupt_received)
      return;

    const float dot_x = cosf(a * 2.0f * M_PI) * r;
    const float dot_y = sinf(a * 2.0f * M_PI) * r;

    canvas->SetPixel(center_x + (int)dot_x,
                     center_y + (int)dot_y,
                     255, 0, 0);  // red spiral

    usleep(1 * 1000);  // slow down a bit
  }
}

int main(int argc, char *argv[]) {
  RGBMatrix::Options defaults;
  defaults.hardware_mapping = "regular";  // change if you use a HAT ("adafruit-hat", etc.)
  defaults.rows = 64;                     // each panel is 64 rows
  defaults.chain_length = 1;              // one panel in the chain
  defaults.parallel = 1;                  // one chain
  defaults.show_refresh_rate = true;

  // Some 64x64 panels need this; you can tweak on command-line instead if needed.
  // defaults.multiplexing = 0;  // auto / default

  Canvas *canvas = RGBMatrix::CreateFromFlags(&argc, &argv, &defaults);
  if (canvas == nullptr)
    return 1;

  signal(SIGTERM, InterruptHandler);
  signal(SIGINT,  InterruptHandler);

  DrawOnCanvas(canvas);

  canvas->Clear();
  delete canvas;
  return 0;
}
