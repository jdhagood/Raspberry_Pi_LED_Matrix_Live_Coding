#!/usr/bin/env python3
import socket
import time
from rgbmatrix import RGBMatrix, RGBMatrixOptions

# --- LED WALL CONFIG ---
WIDTH  = 256   # 4 panels * 64
HEIGHT = 192   # 3 panels * 64

# rpi-rgb-led-matrix options
def make_matrix():
    opts = RGBMatrixOptions()
    opts.rows = 64           # per panel
    opts.cols = 64           # per panel
    opts.chain_length = 4    # 4 panels wide
    opts.parallel = 3        # 3 chains tall
    opts.hardware_mapping = "regular"
    opts.brightness = 100     # tweak as you like
    opts.show_refresh_rate = 3
    return RGBMatrix(options=opts)

# --- UDP CONFIG ---
LISTEN_IP   = "0.0.0.0"
LISTEN_PORT = 9999  # Changed to match your server.py

# New packet header format (12 bytes):
# 2 bytes: width
# 2 bytes: height
# 2 bytes: chunk_idx
# 2 bytes: num_chunks
# 4 bytes: offset

def main():
    matrix = make_matrix()
    offscreen = matrix.CreateFrameCanvas()
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1_000_000)
    sock.bind((LISTEN_IP, LISTEN_PORT))
    
    print(f"Listening for UDP frames on {LISTEN_IP}:{LISTEN_PORT}")
    print(f"Expecting frames of size {WIDTH}x{HEIGHT}")
    
    # Frame assembly state
    frame_buffer = {}
    current_frame_chunks = {}
    last_swap_time = time.time()
    frame_timeout = 0.1  # 100ms timeout for incomplete frames
    
    try:
        while True:
            data, addr = sock.recvfrom(65535)
            
            if len(data) < 12:
                continue
            
            # Parse header
            width = int.from_bytes(data[0:2], "big")
            height = int.from_bytes(data[2:4], "big")
            chunk_idx = int.from_bytes(data[4:6], "big")
            num_chunks = int.from_bytes(data[6:8], "big")
            offset = int.from_bytes(data[8:12], "big")
            chunk_data = data[12:]
            
            # Sanity check
            if width != WIDTH or height != HEIGHT:
                print(f"Wrong dimensions: {width}x{height}, expected {WIDTH}x{HEIGHT}")
                continue
            
            # Store chunk
            frame_key = (width, height, num_chunks)
            if frame_key not in current_frame_chunks:
                current_frame_chunks[frame_key] = {}
            
            current_frame_chunks[frame_key][chunk_idx] = (offset, chunk_data)
            
            # Check if frame is complete
            if len(current_frame_chunks[frame_key]) == num_chunks:
                # Reassemble frame
                total_size = WIDTH * HEIGHT * 3
                framebuffer = bytearray(total_size)
                
                for idx, (off, chunk) in current_frame_chunks[frame_key].items():
                    end = off + len(chunk)
                    if end <= total_size:
                        framebuffer[off:end] = chunk
                
                # Draw to LED matrix
                idx = 0
                for y in range(HEIGHT):
                    for x in range(WIDTH):
                        r = framebuffer[idx + 0]
                        g = framebuffer[idx + 1]
                        b = framebuffer[idx + 2]
                        offscreen.SetPixel(x, y, r, g, b)
                        idx += 3
                
                offscreen = matrix.SwapOnVSync(offscreen)
                last_swap_time = time.time()
                
                # Clear buffer for next frame
                current_frame_chunks.clear()
            
            # Timeout old incomplete frames
            elif time.time() - last_swap_time > frame_timeout:
                current_frame_chunks.clear()
    
    except KeyboardInterrupt:
        print("\nExiting.")
    finally:
        matrix.Clear()
        sock.close()

if __name__ == "__main__":
    main()
