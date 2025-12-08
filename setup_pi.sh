#!/bin/bash
set -e

echo "=== LED Matrix Live Coding Pi Setup ==="

# -------------------------
# 1. System dependencies
# -------------------------
echo "Installing system packages..."
sudo apt update
sudo apt install -y \
  git \
  build-essential \
  python3-dev \
  python3-venv \
  python3-pil \
  libjpeg-dev \
  libopenjp2-7 \
  libwebp-dev \
  unzip


# -------------------------
# 2. Git submodules
# -------------------------
echo "Updating submodules..."
git submodule update --init --recursive

# -------------------------
# 3. Python virtual environment
# -------------------------
echo "Creating Python virtual environment..."
python3 -m venv venv
source venv/bin/activate

echo "Upgrading pip..."
pip install --upgrade pip wheel

# -------------------------
# 4. Python dependencies
# -------------------------
echo "Installing Python packages..."
pip install -r requirements.txt

# -------------------------
# 5. Build rpi-rgb-led-matrix (Python)
# -------------------------
echo "Building rgbmatrix Python bindings..."
cd external/rpi-rgb-led-matrix
pip install .
cd ../..

# -------------------------
# 6. Build C++ programs
# -------------------------
echo "Building C++ matrix binaries..."
make

# -------------------------
# 7. System tuning reminders
# -------------------------
echo ""
echo "IMPORTANT SYSTEM TUNING (DO MANUALLY):"
echo ""
echo "1) Disable built-in audio:"
echo "   sudo nano /boot/firmware/config.txt"
echo "   add:"
echo "     dtparam=audio=off"
echo ""
echo "2) Add CPU isolation to:"
echo "   sudo nano /boot/firmware/cmdline.txt"
echo "   add at end of line:"
echo "     isolcpus=3"
echo ""
echo "3) Reboot after this:"
echo "   sudo reboot"
echo ""

# ---------------
# 8. Blacklisting Pi Audio Drivers
# ----------------
echo "blacklist snd_bcm2835"           | sudo tee  /etc/modprobe.d/blacklist-rgbmatrix-audio.conf
echo "blacklist snd_soc_hdmi_codec"    | sudo tee -a /etc/modprobe.d/blacklist-rgbmatrix-audio.conf
echo "blacklist snd_soc_core"          | sudo tee -a /etc/modprobe.d/blacklist-rgbmatrix-audio.conf



echo "=== Setup Complete ==="
