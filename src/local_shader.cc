// local_shader.cc
// Render simple fragment-style "shaders" locally on the Pi and send directly
// to the rpi-rgb-led-matrix. No web server, no streaming.

#include "led-matrix.h"

#include <signal.h>
#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using rgb_matrix::RGBMatrix;
using rgb_matrix::FrameCanvas;

static volatile bool interrupt_received = false;
static void InterruptHandler(int) {
  interrupt_received = true;
}

static const int WIDTH  = 256;  // 4 * 64
static const int HEIGHT = 192;  // 3 * 64

// Simple “shader” helpers

static void rings_shader(int x, int y, float t, uint8_t &r, uint8_t &g, uint8_t &b) {
  float u = (float)x / (WIDTH - 1);
  float v = (float)y / (HEIGHT - 1);
  // Centered coords
  float px = (u - 0.5f) * 2.0f;
  float py = (v - 0.5f) * 2.0f;

  float d = std::sqrt(px * px + py * py);
  float ring = 0.5f + 0.5f * std::cos(10.0f * d - t * 6.28318f);

  float cr = ring;
  float cg = 0.5f + 0.5f * std::sin(t + px * 4.0f);
  float cb = 0.5f + 0.5f * std::sin(t + py * 4.0f);

  // clamp
  cr = std::fmin(std::fmax(cr, 0.0f), 1.0f);
  cg = std::fmin(std::fmax(cg, 0.0f), 1.0f);
  cb = std::fmin(std::fmax(cb, 0.0f), 1.0f);

  r = (uint8_t)(cr * 255.0f);
  g = (uint8_t)(cg * 255.0f);
  b = (uint8_t)(cb * 255.0f);
}

static void plasma_shader(int x, int y, float t, uint8_t &r, uint8_t &g, uint8_t &b) {
  float u = (float)x / (WIDTH - 1);
  float v = (float)y / (HEIGHT - 1);

  // map to [-1,1]
  float px = (u - 0.5f) * 2.0f;
  float py = (v - 0.5f) * 2.0f;

  float val = 0.0f;
  val += std::sin(px * 3.0f + t * 0.7f);
  val += std::sin(py * 4.0f - t * 1.3f);
  val += std::sin((px + py) * 5.0f + t * 0.5f);
  val /= 3.0f;

  float angle = 6.28318f * (val + 0.0f);
  float cr = 0.5f + 0.5f * std::cos(angle);
  float cg = 0.5f + 0.5f * std::cos(angle + 2.094f);   // +120°
  float cb = 0.5f + 0.5f * std::cos(angle + 4.188f);   // +240°

  cr = std::fmin(std::fmax(cr, 0.0f), 1.0f);
  cg = std::fmin(std::fmax(cg, 0.0f), 1.0f);
  cb = std::fmin(std::fmax(cb, 0.0f), 1.0f);

  r = (uint8_t)(cr * 255.0f);
  g = (uint8_t)(cg * 255.0f);
  b = (uint8_t)(cb * 255.0f);
}

enum ShaderType {
  SHADER_RINGS,
  SHADER_PLASMA
};

int main(int argc, char *argv[]) {
  ShaderType shader = SHADER_RINGS;
  if (argc > 1) {
    if (std::string(argv[1]) == "plasma") {
      shader = SHADER_PLASMA;
    }
  }

  RGBMatrix::Options defaults;
  defaults.hardware_mapping = "regular";
  defaults.rows         = 64;  // per panel
  defaults.cols         = 64;  // per panel
  defaults.chain_length = 4;   // 4 panels per chain
  defaults.parallel     = 3;   // 3 chains in parallel
  defaults.show_refresh_rate = true;

  rgb_matrix::RuntimeOptions rt;
  // use defaults for rt

  RGBMatrix *matrix = RGBMatrix::CreateFromOptions(defaults, rt);
  if (matrix == nullptr) {
    std::fprintf(stderr, "Could not create RGBMatrix\n");
    return 1;
  }

  if (matrix->width() != WIDTH || matrix->height() != HEIGHT) {
    std::fprintf(stderr, "Matrix size is %dx%d (expected %dx%d)\n",
                 matrix->width(), matrix->height(), WIDTH, HEIGHT);
  }

  FrameCanvas *offscreen = matrix->CreateFrameCanvas();

  signal(SIGTERM, InterruptHandler);
  signal(SIGINT,  InterruptHandler);

  const float target_fps = 30.0f;   // you can try 60.0f and see how it feels
  const float frame_dt   = 1.0f / target_fps;
  const useconds_t frame_us = (useconds_t)(frame_dt * 1e6);

  struct timeval start_tv;
  gettimeofday(&start_tv, nullptr);

  std::fprintf(stderr, "Running local shader: %s\n",
               (shader == SHADER_RINGS) ? "rings" : "plasma");

  while (!interrupt_received) {
    struct timeval now;
    gettimeofday(&now, nullptr);
    float t = (now.tv_sec - start_tv.tv_sec) +
              (now.tv_usec - start_tv.tv_usec) / 1e6f;

    for (int y = 0; y < HEIGHT; ++y) {
      for (int x = 0; x < WIDTH; ++x) {
        uint8_t r, g, b;
        switch (shader) {
          case SHADER_RINGS:
            rings_shader(x, y, t, r, g, b);
            break;
          case SHADER_PLASMA:
            plasma_shader(x, y, t, r, g, b);
            break;
        }
        offscreen->SetPixel(x, y, r, g, b);
      }
    }

    offscreen = matrix->SwapOnVSync(offscreen);

    // simple frame cap to reduce CPU load; you can tune or remove
    usleep(frame_us);
  }

  matrix->Clear();
  delete matrix;
  return 0;
}
