#!/bin/bash
# Installs build dependencies for hazard-1 on Raspberry Pi OS (Bullseye/Bookworm,
# 64-bit recommended) and does a release build. Run this ON the Pi, e.g.:
#   scp -r hazard-1 pi@<pi-ip>:~/hazard-1
#   ssh pi@<pi-ip> "cd ~/hazard-1 && bash scripts/rpi_setup.sh"
set -euo pipefail

echo "== hazard-1: Raspberry Pi 4B dependency setup =="

sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git pkg-config \
    qtbase5-dev qt5-qmake qtbase5-dev-tools \
    libopencv-dev \
    v4l-utils

echo
echo "-- Qt version --"
qmake -v || echo "qmake not found on PATH"

echo
echo "-- OpenCV version --"
pkg-config --modversion opencv4 2>/dev/null || pkg-config --modversion opencv 2>/dev/null || echo "opencv pkg-config not found"

echo
echo "-- cv::dnn availability --"
if echo '#include <opencv2/dnn.hpp>
int main(){ return 0; }' | g++ -x c++ - -o /tmp/hazard1_dnn_check $(pkg-config --cflags --libs opencv4 2>/dev/null || pkg-config --cflags --libs opencv) 2>/tmp/hazard1_dnn_check.log; then
    echo "OK: opencv2/dnn.hpp is available and links."
else
    echo "WARNING: could not compile a trivial program against opencv2/dnn.hpp."
    cat /tmp/hazard1_dnn_check.log
fi

echo
echo "-- USB video devices --"
v4l2-ctl --list-devices || echo "No V4L2 devices found (is the USB camera plugged in?)"

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo
echo "-- Configuring --"
cmake -DCMAKE_BUILD_TYPE=Release ..

echo
echo "-- Building (this takes a few minutes on a Pi 4B) --"
cmake --build . -- -j"$(nproc)"

echo
echo "Build complete: $BUILD_DIR/hazard-1"
echo "Run it with, e.g.: $BUILD_DIR/hazard-1 --camera 0"
