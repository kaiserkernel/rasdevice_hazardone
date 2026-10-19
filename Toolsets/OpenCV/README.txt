OpenCV 4.14.0 - Prebuilt Windows binaries (official installer)
================================================================

Version:  4.14.0
Purpose:  Provides the prebuilt OpenCV C++ libraries/headers (vc16 ABI,
          compatible with VS2017/VS2019 MSVC toolsets) needed to build the
          hazard-1 fire/smoke detection app's OpenCV dependency, without
          having to compile OpenCV from source.
Source:   https://github.com/opencv/opencv/releases/download/4.14.0/opencv-4.14.0-windows.exe
          (official OpenCV GitHub Releases page: https://github.com/opencv/opencv/releases)

File:     opencv-4.14.0-windows.exe  (~203 MB, self-extracting 7z/NSIS archive)

How to install on the target machine:
  1. Run:  opencv-4.14.0-windows.exe -oC:\opencv -y
           (the -o<dir> flag sets the extraction directory; -y auto-confirms;
           no admin rights or internet connection are required - it just
           self-extracts, it does not run a "setup wizard" that touches the
           registry)
  2. After extraction you will have:
           <extract-dir>\opencv\build\include            <- headers
           <extract-dir>\opencv\build\x64\vc16\bin        <- DLLs + opencv_world4140.dll
           <extract-dir>\opencv\build\x64\vc16\lib        <- opencv_world4140.lib / .lib
           <extract-dir>\opencv\build\OpenCVConfig.cmake  <- CMake package config
  3. Point CMake at it with:
           -DOpenCV_DIR=<extract-dir>\opencv\build
     or in CMakeLists.txt:
           set(OpenCV_DIR "<extract-dir>/opencv/build")
           find_package(OpenCV REQUIRED)
  4. Add <extract-dir>\opencv\build\x64\vc16\bin to PATH (or copy
     opencv_world4140.dll next to your .exe) so the app can find the DLL
     at runtime.

Note: this is OpenCV 4.14.0, built against the vc16 (VS2017/2019) ABI only
- there is no vc15-only subfolder in this release (recent OpenCV releases
dropped the separate vc14/vc15 binaries and ship a single vc16-labelled
build that is ABI-compatible with both VS2017 and VS2019 MSVC toolsets).
This was verified working against VS2019 Build Tools (MSVC 19.29 / v142
toolset) during validation on 2026-10-18.
