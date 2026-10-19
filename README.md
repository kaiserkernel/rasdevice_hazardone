# hazard-1

Indoor smoke/fire detection from a USB camera, built with Qt 5.14.2 (C++/Widgets)
and OpenCV. The detector is a small YOLO-style object detector trained on the
[Home-fire-dataset](https://github.com/PengBo0/Home-fire-dataset) and run
on-device through OpenCV's `cv::dnn` module (no PyTorch/ONNX Runtime required
at inference time).

> **Status**: functional desktop build, validated against recorded video on a
> Windows dev box; deployment/testing on real Raspberry Pi 4B hardware with a
> live USB camera is tracked separately (see `docs/DISCUSSION.md` for the
> current state of that work). Don't treat this as a certified fire-alarm
> product — see [Limitations & disclaimers](#limitations--disclaimers).

---

## 1. Technical specification

### 1.1 Architecture

```
┌──────────────────────────────────────────────────────────┐
│                         MainWindow (Qt Widgets)           │
│  video QLabel │ alert banner │ toolbar │ status bar │ log │
└───────────────────────────┬────────────────────────────── ┘
                             │ Qt signals/slots (queued, cross-thread)
┌───────────────────────────┴───────────────────────────────┐
│                  DetectionWorker (QThread)                 │
│  1. cv::VideoCapture::read()  -> latest BGR frame           │
│  2. every Nth frame: DetectionEngine::detect(frame)         │
│  3. draw last-known boxes on every frame (no flicker)       │
│  4. AlertManager::processFrame(fireSeen, smokeSeen)         │
│  5. emit frameReady(QImage, fps, inferenceMs)               │
└───────────────────────────┬───────────────────────────────┘
                             │
          ┌──────────────────┴───────────────────┐
          │                                       │
┌─────────┴──────────┐                 ┌──────────┴──────────┐
│  DetectionEngine     │                 │   AlertManager      │
│  cv::dnn::Net (ONNX) │                 │   N-frame debounce,  │
│  letterbox→blob→     │                 │   event log file     │
│  forward→decode→NMS  │                 └──────────────────────┘
└──────────────────────┘
```

Key design choices and why:

- **OpenCV `cv::dnn` instead of ONNX Runtime/libtorch.** The Pi only needs
  `libopencv-dev` from apt; no extra ML runtime to cross-compile or vendor.
  The trade-off is `cv::dnn` requires a fixed-shape ONNX export (see
  `training/`), and is slower than a dedicated ARM-optimized runtime
  (NCNN/TFLite) -- acceptable here since a fire alarm needs a few FPS, not 30.
- **Capture and inference decoupled by frame-skipping, not by two threads.**
  A single `DetectionWorker` thread captures every frame (so the live video
  never stutters waiting on the network) but only runs the detector every
  `N` frames (configurable in the UI, default 3). The most recent detection
  boxes are redrawn on every displayed frame so they don't flicker between
  inference cycles. This matters specifically on a Pi 4B, where YOLO
  inference is far slower than the camera's native frame rate.
- **Per-class NMS.** Fire and smoke boxes commonly overlap in the same region
  (smoke rising off a flame); non-max suppression is run separately per class
  so a fire box doesn't suppress a legitimate smoke box in the same area.
- **N-consecutive-frame alert debounce.** A single frame with a spurious
  detection (lighting flicker, camera noise) does not raise an alert;
  `AlertManager` requires 5 consecutive detecting frames before going active,
  and 5 consecutive clean frames before clearing. Events are timestamped and
  appended to `logs/alerts.log`.
- **Camera *or* video file as source.** The same `DetectionWorker` can open a
  USB camera index or a video file. This is what makes the pipeline testable
  on a machine with no camera attached at all (see `docs/DISCUSSION.md`).

### 1.2 Model

- Architecture: YOLOv8n (Ultralytics), 2 classes: `fire`, `smoke`.
- Input: fixed 320x320, letterboxed (aspect-ratio-preserving pad), RGB, 1/255 scale.
- Export: ONNX, opset 12, static shape (`dynamic=False`) — required for
  `cv::dnn::readNetFromONNX` to reliably parse the graph.
- Training data: Home-fire-dataset, 6,500 images, YOLO-format labels,
  6:2:2 train/val/test split, license **CC BY-NC 4.0** (non-commercial,
  attribution required — see [Licensing](#5-licensing)).
- Full training configuration, exact class index mapping, and measured
  precision/recall/mAP: **`training/TRAINING_REPORT.md`**.

> Training ran CPU-only (no GPU on the dev box) for a time-boxed number of
> epochs — treat this as a working, demo-grade model, not a tuned production
> detector. `training/TRAINING_REPORT.md` is explicit about what more
> epochs/a GPU/a larger input size would likely improve.

### 1.3 Runtime dependencies

| Component | Windows (dev/test) | Raspberry Pi OS (deploy) |
|---|---|---|
| Qt | 5.14.2 (Widgets only) | apt `qtbase5-dev` (5.15.x on current Raspberry Pi OS — see §4.5) |
| OpenCV | 4.x prebuilt (vc15) | apt `libopencv-dev` |
| Compiler | MSVC (v141/v142 toolset) | GCC (system default) |
| Build system | CMake ≥ 3.10 | CMake ≥ 3.10 |

---

## 2. Project structure

```
hazard-1/
├── CMakeLists.txt            # Qt5 + OpenCV CMake build, same file for Windows and Pi
├── src/
│   ├── main.cpp               # QApplication + CLI args (--model --classes --camera --video)
│   ├── MainWindow.{h,cpp}     # UI: video view, alert banner, toolbar, status bar, log dock
│   ├── DetectionWorker.{h,cpp}# QThread: capture + throttled inference + frame annotation
│   ├── DetectionEngine.{h,cpp}# cv::dnn wrapper: letterbox, forward pass, decode, per-class NMS
│   ├── AlertManager.{h,cpp}   # N-frame debounce + event log
│   └── Detection.h            # POD: classId, className, confidence, box
├── models/
│   ├── fire_smoke_yolov8n.onnx
│   └── classes.names          # one class name per line, index order must match training
├── training/
│   ├── data.yaml               # YOLO dataset config used for training
│   ├── requirements.txt        # exact pip freeze of the training venv
│   ├── train_log.txt           # raw training stdout
│   └── TRAINING_REPORT.md      # dataset, hyperparameters, metrics, caveats
├── scripts/
│   ├── rpi_setup.sh            # apt deps + CMake build, run ON the Pi
│   ├── hazard-1-autostart.desktop  # simplest autostart (Raspberry Pi OS Desktop)
│   └── hazard-1.service        # optional systemd-supervised alternative
├── docs/
│   └── DISCUSSION.md           # design decisions, open issues, what was/wasn't verified
├── Toolsets/                   # non-Qt/non-VS tools used to build this project (see its README)
└── README.md                   # this file
```

---

## 3. Building

### 3.1 Windows (development/testing build)

Prerequisites: Qt 5.14.2 (MSVC kit), a VS2017/2019 C++ toolchain, CMake, and
OpenCV (prebuilt Windows binaries). If you don't already have these, see
`Toolsets/` for what to install (Qt and Visual Studio itself are assumed
already present, per project scope).

```powershell
cmake -S . -B build -G "Visual Studio 16 2019" -A x64 `
    -DCMAKE_PREFIX_PATH="D:\Qt\5.14.2\msvc2017_64;D:\opencv\opencv\build"
cmake --build build --config Release
```

Qt's and OpenCV's DLLs (`Qt5Core.dll`, `Qt5Widgets.dll`, `opencv_world*.dll`,
etc.) must be either on `PATH` or copied next to `hazard-1.exe` before
running — `windeployqt` handles the Qt side:

```powershell
D:\Qt\5.14.2\msvc2017_64\bin\windeployqt.exe build\Release\hazard-1.exe
```

Run it against a video file (no camera needed) to validate the pipeline:

```powershell
build\Release\hazard-1.exe --video path\to\sample_fire_video.mp4
```

### 3.2 Raspberry Pi 4B (deployment build)

See **§4 Raspberry Pi 4B porting tutorial** below — in short:

```bash
scp -r hazard-1 pi@<pi-ip>:~/hazard-1
ssh pi@<pi-ip> "cd ~/hazard-1 && bash scripts/rpi_setup.sh"
```

---

## 4. Raspberry Pi 4B porting tutorial

This section assumes a Raspberry Pi 4B running Raspberry Pi OS (Bullseye or
Bookworm, 64-bit recommended for better OpenCV/DNN performance), with a
UVC-compatible USB camera, reachable over SSH.

### 4.1 Flash and first boot

1. Flash Raspberry Pi OS (with Desktop) to an SD card with Raspberry Pi
   Imager. Enable SSH and configure Wi-Fi/user account in the imager's
   advanced options (gear icon) so it's reachable headlessly on first boot.
2. Boot the Pi, find its IP (router DHCP leases, or `ping raspberrypi.local`),
   and confirm SSH access: `ssh pi@<pi-ip>`.
3. Plug in the USB camera and confirm Linux sees it:
   ```bash
   lsusb
   ls /dev/video*
   ```
   A UVC webcam typically shows up as `/dev/video0` (and often a second
   `/dev/video1` metadata node — try index 0 first, then 1 if the picture is wrong).

### 4.2 Copy the project and install dependencies

```bash
scp -r hazard-1 pi@<pi-ip>:~/hazard-1
ssh pi@<pi-ip>
cd ~/hazard-1
bash scripts/rpi_setup.sh
```

`rpi_setup.sh` installs `build-essential cmake git pkg-config qtbase5-dev
qt5-qmake qtbase5-dev-tools libopencv-dev v4l-utils`, prints the installed Qt/
OpenCV versions, sanity-checks that `opencv2/dnn.hpp` actually compiles and
links (some minimal OpenCV packages omit the dnn module — this check catches
that early), lists detected V4L2 devices, and then configures+builds the
project with CMake into `~/hazard-1/build/hazard-1`.

### 4.3 Run it

```bash
cd ~/hazard-1/build
./hazard-1 --camera 0
```

This is a GUI (Qt Widgets) app, so it needs a display — run it from the Pi's
desktop session (either logged in directly with a monitor attached, or over
VNC/RDP to the Pi's desktop; a plain SSH terminal with no X session won't show
the window). Use `--video <path>` instead of `--camera` to sanity-check the
pipeline with a recorded clip before trusting a live camera.

### 4.4 Autostart on boot

Simplest option — Desktop autostart entry (recommended for a kiosk-style
always-on monitor):

```bash
mkdir -p ~/.config/autostart
cp scripts/hazard-1-autostart.desktop ~/.config/autostart/
```

Edit the `Exec=` line in that file first if your project isn't at
`/home/pi/hazard-1`. For process supervision with auto-restart instead, use
`scripts/hazard-1.service` (systemd) — see the comments in that file for
install steps; it requires desktop autologin to be enabled
(`sudo raspi-config` → System Options → Boot / Auto Login → Desktop Autologin)
since the service still needs an X session to render into.

### 4.5 Known Pi-specific gotchas

- **Exact Qt version.** Raspberry Pi OS's `qtbase5-dev` package is **not**
  exactly 5.14.2 (Bullseye ships 5.15.2, Bookworm ships a later 5.15.x) —
  there is no official apt package pinned to 5.14.2 for Debian/Raspberry Pi
  OS. This app uses no API introduced after 5.14, so building against the
  apt-provided 5.15.x is expected to work unmodified. If you need the *exact*
  5.14.2 version (e.g. for strict parity with the Windows dev build), you
  must build Qt 5.14.2 from source on the Pi — expect several hours; see
  `docs/DISCUSSION.md` for notes on why this wasn't done by default.
- **`cv::dnn` must be present.** A handful of minimal/custom OpenCV builds
  exclude the `dnn` module. `rpi_setup.sh` checks for this; if it fails,
  install the full `libopencv-dev` (not a `-core`-only variant) or build
  OpenCV from source with `-DBUILD_opencv_dnn=ON`.
- **Camera device index.** `/dev/video0` isn't guaranteed to be the actual
  capture node on every UVC camera (some expose metadata on a lower index) —
  if the image looks wrong or the camera fails to open, try `--camera 1`.
- **Inference speed.** Expect roughly 1-4 FPS of actual detection throughput
  on a Pi 4B CPU with the 320x320 YOLOv8n model (video display itself stays
  smooth regardless, since display isn't gated on detection — see §1.1). This
  is a CPU-only, non-NEON-optimized-runtime number; see
  `docs/DISCUSSION.md` for ideas on speeding this up further.
- **USB camera dropouts.** `DetectionWorker` already retries the camera after
  15 consecutive failed reads (1s backoff) rather than giving up — relevant on
  the Pi where USB power/bandwidth issues are more common than on a desktop.

---

## 5. Licensing

This project combines pieces with **different licenses** — read before
redistributing:

- **Home-fire-dataset**: CC BY-NC 4.0 (non-commercial, attribution required).
  Any model trained on it, and any derivative work built from that model,
  inherits the non-commercial restriction. Cite: Peng, B. & Kim, T.-K.,
  "YOLO-HF: Early Detection of Home Fires Using YOLO," *IEEE Access*, 2025,
  DOI 10.1109/ACCESS.2025.3566907.
- **Ultralytics YOLOv8** (used only for *training*, not at runtime): AGPL-3.0.
  hazard-1's runtime (`cv::dnn` loading an exported `.onnx` file) does not
  link against or invoke Ultralytics code, so the deployed application itself
  is not directly AGPL-encumbered by that dependency — but the *training
  pipeline* in `training/` is, and anyone extending training should keep that
  in mind. This is not legal advice; verify independently before any
  commercial use.
- **Qt 5.14.2** and **OpenCV**: see their own licenses (LGPL/GPL depending on
  Qt edition and modules used; BSD/Apache for OpenCV).

---

## 6. Limitations & disclaimers

hazard-1 is a learning/demo project, not a certified life-safety device:

- The model is trained on a relatively small, CPU-time-boxed run on a
  non-commercial dataset; false negatives and false positives should be
  expected. **Do not rely on this as a substitute for a certified smoke/heat
  detector.**
- Real USB-camera + Raspberry Pi 4B hardware testing status is tracked in
  `docs/DISCUSSION.md` — check there for the current state before assuming
  the full pipeline has been validated end-to-end on real hardware.
- Lighting, camera placement/FOV, and camera quality all materially affect
  detection accuracy; this was tuned/tested against the Home-fire-dataset's
  image distribution, not any particular physical installation.
