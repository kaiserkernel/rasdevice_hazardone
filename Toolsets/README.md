# Toolsets for hazard-1 (for a fresh machine that already has Qt 5.14.2 + VS2019)

This folder is meant to be copied to a different, fresh Windows machine -
one that **already has** Qt 5.14.2 and the full Visual Studio 2019 IDE
installed (the user's own machine/license) - and is missing everything
else needed to build the hazard-1 Qt5 + OpenCV C++ application.

## What's inside

- `OpenCV\opencv-4.14.0-windows.exe` - the official prebuilt OpenCV 4.14.0
  Windows self-extracting package (vc16 ABI, works with VS2017/2019).
  See `OpenCV\README.txt` for the exact extraction/CMake setup commands.

- `CMake\cmake-4.4.4-windows-x86_64.msi` - the standalone official CMake
  installer. See `CMake\README.txt` for why it's included: the
  VS Build Tools "VCTools" workload used in this validation session did
  NOT bundle cmake.exe even with --includeRecommended, so a standalone
  CMake was needed to configure/build the project. If the target machine's
  full VS2019 IDE install already has the "C++ CMake tools for Windows"
  component (it does by default for a normal "Desktop development with
  C++" IDE install), this MSI is redundant and can be skipped - check for
  `...\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
  under the VS2019 install directory first.

## What's deliberately NOT inside, and why

- **Visual Studio Build Tools / vs_buildtools.exe** - not included. The
  target machine already has the full Visual Studio 2019 IDE with its own
  MSVC compiler/linker/Windows SDK, which is a superset of what Build
  Tools provides. Installing Build Tools on top would be redundant and
  could create confusing duplicate VS instances (which is in fact what
  happened during this validation session - see TOOLCHAIN_REPORT.md).

- **Qt 5.14.2 (or the aqtinstall tool)** - not included. The target
  machine already has a licensed Qt 5.14.2 install. aqtinstall was only
  used in this disposable validation VM as a scriptable stand-in for the
  interactive Qt installer (no Qt account available in this environment),
  purely to prove the Qt+OpenCV+CMake toolchain works end-to-end before
  deployment. It has no purpose on a machine that already has Qt.

## Source of truth

See `D:\hazard-1-buildcheck\TOOLCHAIN_REPORT.md` (on the validation VM)
for the full validation log: exact install paths, CMAKE_PREFIX_PATH /
Qt5_DIR / OpenCV_DIR values, the configure+build command sequence, and
confirmation that the Qt5+OpenCV smoke-test app built and ran successfully.
