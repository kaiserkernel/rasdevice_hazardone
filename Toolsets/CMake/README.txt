CMake 4.4.4 - standalone Windows installer
===========================================

Version:  4.4.4
Purpose:  CMake is required to configure/build the hazard-1 Qt5+OpenCV
          project (CMakeLists.txt-based). This standalone installer is
          included because it was NOT bundled with "Build Tools for Visual
          Studio 2019" in this validation session - see note below.
Source:   https://github.com/Kitware/CMake/releases/latest
          (official Kitware CMake releases page: https://cmake.org/download/)
File:     cmake-4.4.4-windows-x86_64.msi  (~39 MB, standard MSI installer)

Install: run the MSI and choose "Add CMake to the system PATH" during
setup (or silently: msiexec /i cmake-4.4.4-windows-x86_64.msi /quiet
ADD_CMAKE_TO_PATH=System).

--- Why this is here (important) ---
On a machine that already has the FULL Visual Studio 2019 IDE with the
"Desktop development with C++" workload, cmake.exe IS normally bundled
(as the "C++ CMake tools for Windows" component, installed by default
with that workload in the IDE installer's UI) - typically at:
  C:\Program Files (x86)\Microsoft Visual Studio\2019\<Edition>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe

HOWEVER, during this validation session we installed the standalone
"Build Tools for Visual Studio 2019" (Microsoft.VisualStudio.Workload.VCTools)
via the command-line bootstrapper, and even with --includeRecommended
passed, the resulting install did NOT include cmake.exe anywhere under
the install tree (confirmed by recursive search - only vcvars64.bat,
cl.exe, the MSVC 14.29 toolset and the Windows 10 SDK were present).
The CMake/CMake-Project component for VC Tools is "recommended", not
"default", for the Build Tools SKU, and in practice it did not get pulled
in. We worked around this in-session by using the "cmake" PyPI package
(pip install cmake -> cmake 4.4.4) which was already present via Python,
but that is a pip-only workaround, not something to rely on for a bare
machine. For the real target machine (which already has the full VS2019
IDE, not Build Tools), FIRST check whether
  C:\Program Files (x86)\Microsoft Visual Studio\2019\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
already exists - if it does, this installer is not needed. If it does
not (e.g. CMake tools component was deselected during the original VS
install), use this standalone installer instead.
