# hazard-1 — project discussion log

This document records the decisions, open questions, and constraints behind
hazard-1, in the order they came up. It exists so that the *why* behind
non-obvious choices doesn't get lost once the code is the only thing left.

## 1. Starting premise vs. what was actually found

The brief was "clone Home-fire-dataset, build hazard-1 around its model, and
build+test it on a Raspberry Pi 4B." Before writing any code, three premises
turned out not to hold, and each changed the plan materially:

1. **"...with the logic and the model of the project."** The
   [Home-fire-dataset repo](https://github.com/PengBo0/Home-fire-dataset) is
   **dataset-only** — 6,500 YOLO-annotated images (CC BY-NC 4.0 license), with
   no training code, no inference code, and no model weights anywhere in it.
   There was no existing "logic" or "model" to port. This meant a model had to
   be trained from scratch on this data before hazard-1 could do anything —
   see §3.
2. **"...build it for RaspberryPi4B and test all the features... in
   Raspberry Pi4B."** The environment this work is actually running in is a
   Windows machine with no Raspberry Pi 4B and no USB camera attached. Real
   hardware testing requires either physical access or remote (SSH) access to
   an actual Pi. This was raised explicitly; see §5 for the current state of
   that access.
3. **"I have already visual studio 2019, QT5.14.2..."** — true on the
   requester's own machine, but *this* Windows environment (the one actually
   doing the work) had neither Qt nor any Visual Studio installation anywhere
   on disk (verified: no `qmake.exe` under `C:\`, no
   `Program Files\Microsoft Visual Studio` directory, no `QTDIR`/
   `VSINSTALLDIR` env vars — and the GPU adapter list showed a "Microsoft
   Remote Display Adapter," consistent with this being a disposable/cloud
   session rather than a persistent workstation). Confirmed with the
   requester that this is indeed a temporary box, so Qt 5.14.2 and a minimal
   MSVC toolchain were installed here purely to compile/validate the code —
   see `Toolsets/README.md` for what was and wasn't bundled as a result.

None of this is a criticism of the original ask — a dataset repo reasonably
*sounds* like it could include a reference model, and "I have Qt/VS" is
usually true of the requester's actual desk. It's recorded here because it
explains several downstream choices (training from scratch, a toolchain setup
phase that wouldn't otherwise be needed, and treating Pi testing as a
dependency on remote access rather than something done directly).

## 2. Architecture decisions

### 2.1 OpenCV `cv::dnn` over ONNX Runtime / libtorch for inference

Considered three ways to run the trained model on-device:

- **libtorch** (PyTorch C++): heaviest dependency, painful to cross-build for
  ARM, rejected early.
- **ONNX Runtime**: lighter than libtorch, has ARM builds, but is still an
  extra library to obtain/build for the Pi and link against from Qt — either
  a separate apt package that may not exist for the target Raspberry Pi OS
  release, or a manual build.
- **OpenCV `cv::dnn`** (chosen): the Pi already needs OpenCV for camera
  capture (`cv::VideoCapture`) and image drawing. `libopencv-dev` from apt
  already includes the `dnn` module on a normal (non-minimal) build, so this
  adds *zero* extra dependencies beyond what camera support already requires.
  The real cost is that `cv::dnn`'s ONNX importer is less forgiving — it wants
  a **static input shape**, which is why the export step
  (`training/TRAINING_REPORT.md`) explicitly uses `dynamic=False` and a fixed
  320x320 input rather than Ultralytics' default dynamic-shape export.

### 2.2 One capture thread with frame-skipped inference, not two pipelines

The natural failure mode on a Pi 4B is: camera delivers frames faster than
the network can process them. Two architectures were considered:

- Two independent threads (a capture thread, and a separate detection thread
  that always grabs "whatever the latest frame is"), communicating through a
  mutex-guarded shared frame buffer.
- One thread that captures every frame but only calls the detector every
  `N` frames, always re-drawing the *last known* detection boxes onto every
  displayed frame (chosen).

The two-thread version is more "correct" in the sense that detection always
runs on the freshest possible frame with no artificial N-frame cadence, but
it adds real synchronization complexity (shared `cv::Mat` ownership across
threads) for a benefit that doesn't matter much here — a fire/smoke alarm
does not need the absolute latest frame for its decision, it needs *any*
recent frame processed without stalling the live video. The simpler
single-thread/frame-skip design was picked deliberately over the
"more correct" alternative because the complexity wasn't buying anything
the use case needs. `N` is exposed as a UI control specifically so it can be
tuned once real Pi 4B inference timing is known (see §5).

### 2.3 Per-class NMS, not global NMS

Fire and smoke detections frequently overlap in the same screen region
(smoke rises directly off visible flame). Running a single NMS pass across
all classes together would let a confident fire box suppress a legitimate,
separately-useful smoke box nearby. NMS is run once per class
(`DetectionEngine::detect`, grouping by `classId` before calling
`cv::dnn::NMSBoxes`) so both can be reported independently when both are
genuinely present.

### 2.4 N-consecutive-frame alert debounce

A YOLO detector run on live, noisy camera video will occasionally fire a
single spurious frame (motion blur, a reflection, a lighting change). Raising
a visible/audible alert on frame one would make the UI feel unreliable.
`AlertManager` requires 5 consecutive detecting frames before an alert state
goes active, and 5 consecutive clean frames before it clears — cheap to
implement, and avoids a flickering alert banner. This threshold is a
reasonable starting default, not a value derived from measured false-positive
rates on this specific model; the training report's precision/recall numbers
are the better signal for whether it should be tuned.

### 2.5 Camera *or* video file as the same input abstraction

`DetectionWorker::openCamera` and `openVideoFile` both ultimately just call
`cv::VideoCapture::open` with different arguments, and the rest of the
pipeline doesn't know or care which one is active. This was not a
nice-to-have: it is what made it possible to exercise the entire
capture→detect→render→alert pipeline on a machine with **no camera at all**
by substituting a recorded fire/smoke video, which is exactly the situation
this work was done under (see §4).

## 3. Training the model

Since no model existed in the source dataset repo, one had to be trained.
Full details (exact hyperparameters, class index mapping, measured metrics,
and what would improve it) are in `training/TRAINING_REPORT.md` — this
section only covers the *decisions*, not the results:

- **CPU-only, time-boxed training.** The available machine has no NVIDIA GPU.
  Rather than attempt a long, unattended multi-day CPU training run,
  training was explicitly time-boxed (a short timing probe on 1 epoch, then
  an epoch budget chosen to land training in roughly a 2-5 hour window). This
  is a deliberate accuracy-vs-time trade-off: the resulting model is
  demo-grade, not a tuned production detector, and `TRAINING_REPORT.md` says
  so plainly rather than overstating the result.
- **YOLOv8n over larger YOLOv8 variants.** The smallest Ultralytics model
  (`yolov8n`) was chosen specifically because the deployment target is a
  Raspberry Pi 4B's CPU — a larger/more accurate variant would train to a
  better mAP but cost more at inference time on exactly the hardware this
  needs to run acceptably fast on.
- **Fixed 320x320 input.** Chosen as a balance between retaining enough
  detail for *early-stage, small* flames/smoke (the dataset's own stated
  focus) and keeping Pi 4B CPU inference time tolerable. This is a
  judgment call, not a measured optimum — if Pi 4B testing (§5) shows
  inference is comfortably fast, a larger input (e.g. 416) would likely
  improve small-object recall at some FPS cost, and that trade-off is worth
  revisiting once real on-device timing is known.
- **License propagation.** Because the dataset is CC BY-NC 4.0, the trained
  weights and anything built from them inherit the non-commercial
  restriction. This is called out in the top-level `README.md` rather than
  only here, since it's a constraint on how hazard-1 as a whole may be used,
  not just a training detail.

## 4. What was and wasn't testable without real hardware

The development machine had neither a Raspberry Pi 4B nor a USB camera
attached. Rather than claim untested things were tested, the pipeline was
validated in the pieces that *could* be exercised locally:

- The capture→detect→render→alert pipeline end-to-end against a **recorded
  video file** standing in for a live camera (`--video <path>` — see
  `README.md` §3.1), which exercises every code path except the actual V4L2
  camera backend and real-camera failure/reconnect behavior.
- A Windows build of the exact same source tree (the `CMakeLists.txt` and all
  `src/` files are platform-agnostic; the only platform-conditional code is
  the `cv::CAP_V4L2` backend selection in `DetectionWorker::openCamera`,
  gated behind `#ifdef HAZARD1_PLATFORM_LINUX`), to catch compile-time and
  basic runtime issues cheaply before ever touching Pi hardware.

What specifically was **not** verified without real Pi/camera access, and
should not be assumed working until it is:

- Actual V4L2 behavior of a specific USB camera on Raspberry Pi OS (device
  node numbering, supported resolutions/formats, exposure under indoor
  lighting).
- Real end-to-end inference latency and achievable FPS on a Pi 4B's CPU —
  the "~1-4 FPS" figure in `README.md` is an estimate based on published
  YOLOv8n/OpenCV-DNN benchmarks on similar ARM hardware, not a measurement
  taken on this project's own Pi.
- Whether the apt-provided Qt 5.15.x on Raspberry Pi OS (there is no 5.14.2
  apt package — see `README.md` §4.5) actually builds this project without
  adjustment; this is expected to be fine given no post-5.14 Qt APIs are
  used, but "expected to be fine" is not the same as "built and run."
- The camera-dropout reconnect logic (`DetectionWorker::run`'s
  consecutive-failure/backoff path) under a real USB camera being physically
  unplugged/power-cycled.

## 5. Raspberry Pi access status

Real build/deploy/test on a physical Raspberry Pi 4B requires either physical
access to the device or SSH access to one that's reachable from the
development machine. This was asked about explicitly; SSH access was offered
but the connection details had not yet been provided as of the most recent
work on this document. **Until that happens, treat everything in §4's
"not verified" list as outstanding work**, not as done. Once access is
available, the remaining steps are exactly `README.md` §4 (copy the project
over, run `scripts/rpi_setup.sh`, run against `--camera 0`, measure real FPS,
and tune the "detect every N frames" default and alert debounce thresholds
against what's actually seen).

## 6. Toolchain setup on the Windows dev box

Because this machine had neither Qt nor a C++ compiler (§1, point 3), and a
full Visual Studio 2019 IDE install would have been redundant with what the
requester already has on their real machine, a **minimal** toolchain was
installed here instead: "Build Tools for Visual Studio 2019" (compiler/linker
only, no IDE) plus Qt 5.14.2 installed headlessly via `aqtinstall` (a
scriptable installer that pulls directly from Qt's official archives,
avoiding the interactive GUI installer and Qt-account login it normally
requires) plus a prebuilt OpenCV Windows package. None of these three are
included in `Toolsets/` (see its own `README.md`) specifically *because* they
were only a stand-in for tools the requester already owns — bundling them
would contradict the point of excluding Qt/VS from that folder.
