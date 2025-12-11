// udp_matrix_receiver.cc
// Receive RGB frames via UDP and display on a 4x3 64x64 HUB75 array (256x192).

#include "led-matrix.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using rgb_matrix::RGBMatrix;
using rgb_matrix::FrameCanvas;

static volatile bool interrupt_received = false;
static void InterruptHandler(int) { interrupt_received = true; }

static const int WIDTH  = 256;  // 4 * 64
static const int HEIGHT = 192;  // 3 * 64
static const size_t FRAME_BYTES = WIDTH * HEIGHT * 3;

static const int UDP_PORT = 5005;           // choose your port
static const size_t CHUNK_SIZE = 1024;      // payload bytes per packet
static const size_t HEADER_SIZE = 6;        // frame_id, packet_idx, total_pkts

struct UdpPacketHeader {
  uint16_t frame_id;
  uint16_t packet_index;
  uint16_t total_packets;
};

// Simple helper to get time
static uint64_t NowMicros() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (uint64_t)tv.tv_sec * 1000000ull + tv.tv_usec;
}

int main(int argc, char *argv[]) {
  // --- Matrix setup (copy your working config from local_shader.cc) ---
  RGBMatrix::Options defaults;
  defaults.hardware_mapping = "regular";
  defaults.rows         = 64;
  defaults.cols         = 64;
  defaults.chain_length = 4;   // 4 panels per chain
  defaults.parallel     = 3;   // 3 chains in parallel
  defaults.show_refresh_rate = true;

  rgb_matrix::RuntimeOptions rt;
  // If you use any special runtime options (e.g., gpio_slowdown), set them here.
  // rt.gpio_slowdown = 2;  // example, if you used that before.

  RGBMatrix *matrix = RGBMatrix::CreateFromOptions(defaults, rt);
  if (!matrix) {
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

  // --- UDP socket setup ---
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket");
    return 1;
  }

  int reuse = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(UDP_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(sock);
    return 1;
  }

  std::fprintf(stderr, "Listening for frames on UDP port %d\n", UDP_PORT);

  // --- Frame reassembly buffers ---
  std::vector<uint8_t> frame_buf(FRAME_BYTES, 0);
  uint16_t current_frame_id = 0;
  uint16_t expected_packets = 0;
  std::vector<bool> got_packet;
  size_t received_packets = 0;

  std::vector<uint8_t> recv_buf(HEADER_SIZE + CHUNK_SIZE);

  while (!interrupt_received) {
    ssize_t n = recv(sock, recv_buf.data(), recv_buf.size(), 0);
    if (n < (ssize_t)HEADER_SIZE)
      continue;

    UdpPacketHeader hdr;
    std::memcpy(&hdr.frame_id,   &recv_buf[0], 2);
    std::memcpy(&hdr.packet_index, &recv_buf[2], 2);
    std::memcpy(&hdr.total_packets, &recv_buf[4], 2);

    // Network byte order to host
    hdr.frame_id = ntohs(hdr.frame_id);
    hdr.packet_index = ntohs(hdr.packet_index);
    hdr.total_packets = ntohs(hdr.total_packets);

    size_t payload_len = n - HEADER_SIZE;
    if (payload_len > CHUNK_SIZE)
      continue;

    // New frame?
    if (hdr.frame_id != current_frame_id) {
      current_frame_id = hdr.frame_id;
      expected_packets = hdr.total_packets;
      got_packet.assign(expected_packets, false);
      received_packets = 0;
      std::fill(frame_buf.begin(), frame_buf.end(), 0);
    }

    if (hdr.packet_index >= expected_packets)
      continue;

    size_t offset = (size_t)hdr.packet_index * CHUNK_SIZE;
    if (offset >= FRAME_BYTES)
      continue;

    size_t copy_len = payload_len;
    if (offset + copy_len > FRAME_BYTES) {
      copy_len = FRAME_BYTES - offset;
    }

    std::memcpy(&frame_buf[offset], &recv_buf[HEADER_SIZE], copy_len);

    if (!got_packet[hdr.packet_index]) {
      got_packet[hdr.packet_index] = true;
      received_packets++;
    }

    // If we have all packets for this frame, draw it.
    if (received_packets == expected_packets) {
      // Render to matrix
      const uint8_t *p = frame_buf.data();
      for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
          uint8_t r = *p++;
          uint8_t g = *p++;
          uint8_t b = *p++;
          offscreen->SetPixel(x, y, r, g, b);
        }
      }
      offscreen = matrix->SwapOnVSync(offscreen);
    }
  }

  close(sock);
  matrix->Clear();
  delete matrix;
  return 0;
}
