@echo off
:: One-step build of the Mumble client on Windows (x64, Release).
:: Works from a plain ZIP download of this repository, no git needed.
:: See BUILD_ON_WINDOWS.md for the requirements.
::
:: Usage: build_windows.bat [--no-pause]
::   --no-pause   don't wait for a key press at the end (for use from scripts)

setlocal EnableExtensions
set "NOPAUSE=%~1"

:: --- Settings --------------------------------------------------------------
:: Prebuilt dependencies (Qt 5, Opus, ...) published by the Mumble project.
:: They must match the Mumble version of this source tree (1.5.x).
set "ENV_NAME=mumble_env.x64-windows-static-md.2025-07-29.9578b83ca8"
set "ENV_URL=https://github.com/mumble-voip/vcpkg/releases/download/2025-07_qt5/%ENV_NAME%.7z"

set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "ENV_STORE=%ROOT%\mumble-build-env"
set "ENV_DIR=%ENV_STORE%\%ENV_NAME%"
set "BUILD=%ROOT%\build"

echo.
echo === Mumble build (Windows x64, Release) ===
echo Source: %ROOT%
echo.

:: --- 1. Visual Studio ------------------------------------------------------
echo [1/5] Looking for Visual Studio with C++ tools...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_vs
set "VSPATH="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH goto :no_vs
if not exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" goto :no_vs
echo       Found: %VSPATH%

:: --- 2. Python 3 -----------------------------------------------------------
echo [2/5] Looking for Python 3...
python -c "import sys; sys.exit(0 if sys.version_info[0] == 3 else 1)" >nul 2>&1
if errorlevel 1 goto :no_python
for /f "delims=" %%v in ('python --version 2^>^&1') do echo       Found: %%v

:: --- 3. Build environment (dependencies) -----------------------------------
echo [3/5] Checking the build environment...
if exist "%ENV_DIR%\scripts\buildsystems\vcpkg.cmake" goto :env_ok
if not exist "%ENV_STORE%" mkdir "%ENV_STORE%"
echo       Not found. Downloading %ENV_NAME%.7z (about 320 MB)...
curl.exe -L --fail --retry 3 -o "%ENV_STORE%\%ENV_NAME%.7z" "%ENV_URL%"
if errorlevel 1 goto :download_failed
echo       Extracting (about 4.5 GB, takes a few minutes)...
"%SystemRoot%\System32\tar.exe" -xf "%ENV_STORE%\%ENV_NAME%.7z" -C "%ENV_STORE%"
if errorlevel 1 goto :extract_failed
if not exist "%ENV_DIR%\scripts\buildsystems\vcpkg.cmake" goto :extract_failed
del "%ENV_STORE%\%ENV_NAME%.7z"
:env_ok
echo       OK: %ENV_DIR%

:: --- 4. Compiler environment + configure -----------------------------------
echo [4/5] Setting up the compiler...
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto :vcvars_failed
:: Use the CMake and Ninja that come with Visual Studio if they aren't on PATH already
set "PATH=%PATH%;%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
where cmake >nul 2>&1
if errorlevel 1 goto :no_cmake
where ninja >nul 2>&1
if errorlevel 1 goto :no_cmake

if not exist "%BUILD%" mkdir "%BUILD%"
if exist "%BUILD%\build.ninja" goto :configured
echo       First build: configuring with CMake...
cmake -S "%ROOT%" -B "%BUILD%" -G Ninja ^
	-DCMAKE_TOOLCHAIN_FILE="%ENV_DIR%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md ^
	-DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe -DCMAKE_BUILD_TYPE=Release ^
	-Dstatic=ON -Dserver=OFF -Dice=OFF -Doverlay-xcompile=OFF -Dpackaging=OFF -Dtests=OFF ^
	-Dasio=OFF -Dg15=OFF -Delevation=OFF -Dupdate=OFF
if errorlevel 1 goto :configure_failed
:configured

:: --- 5. Build ---------------------------------------------------------------
echo [5/5] Building (the first build takes a while)...
:: A running Mumble from this build folder locks its plugin DLLs, which makes linking fail
powershell -NoProfile -Command "if (Get-Process mumble -ErrorAction SilentlyContinue | Where-Object { $_.Path -like '%BUILD%\*' }) { exit 1 }"
if errorlevel 1 goto :mumble_running
cmake --build "%BUILD%" --target mumble
if errorlevel 1 goto :build_failed

echo.
echo === Done ===
echo Run Mumble: "%BUILD%\mumble.exe"
echo (Keep mumble.exe in that folder: it loads the plugins next to it.)
goto :end

:: --- Errors ------------------------------------------------------------------
:no_vs
echo.
echo ERROR: Visual Studio 2022 (or newer) with C++ tools was not found.
echo Install "Visual Studio Community" (2022 or newer) from https://visualstudio.microsoft.com/
echo and tick the workload "Desktop development with C++". Then run this script again.
goto :fail
:no_python
echo.
echo ERROR: Python 3 was not found on PATH.
echo Install Python 3 from https://www.python.org/downloads/ and tick
echo "Add python.exe to PATH" in the installer. Then open a new window and run this script again.
goto :fail
:download_failed
echo.
echo ERROR: Downloading the build environment failed. Check your internet connection.
echo URL: %ENV_URL%
if exist "%ENV_STORE%\%ENV_NAME%.7z" del "%ENV_STORE%\%ENV_NAME%.7z"
goto :fail
:extract_failed
echo.
echo ERROR: Extracting the build environment failed (not enough disk space?).
echo Delete the folder "%ENV_STORE%" and run this script again.
goto :fail
:vcvars_failed
echo.
echo ERROR: Setting up the Visual Studio compiler environment failed.
goto :fail
:no_cmake
echo.
echo ERROR: CMake or Ninja was not found. In the Visual Studio Installer, add the
echo component "C++ CMake tools for Windows", then run this script again.
goto :fail
:configure_failed
echo.
echo ERROR: CMake configuration failed, see the messages above.
echo Before retrying, delete the folder "%BUILD%".
goto :fail
:mumble_running
echo.
echo ERROR: Mumble is running from "%BUILD%". Close it and run this script again.
goto :fail
:build_failed
echo.
echo ERROR: The build failed, see the messages above.
goto :fail

:fail
if /i not "%NOPAUSE%"=="--no-pause" pause
exit /b 1

:end
if /i not "%NOPAUSE%"=="--no-pause" pause
exit /b 0
