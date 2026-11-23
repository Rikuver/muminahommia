@echo off
:: Same as build_windows.bat, but compiles with half the CPU threads so the PC
:: stays usable and runs cooler. Use this if the normal build freezes or
:: overheats your computer. Slower, but otherwise identical.
::
:: Usage: build_windows_low_load.bat [--no-pause] [jobs]
::   jobs   number of parallel compile jobs (default: half your CPU threads)

setlocal EnableExtensions
set /a "JOBS=%NUMBER_OF_PROCESSORS% / 2"
if not "%~2"=="" set /a "JOBS=%~2"
if not "%~1"=="" if /i not "%~1"=="--no-pause" set /a "JOBS=%~1"
if %JOBS% LSS 1 set "JOBS=1"

:: Read by "cmake --build" (Ninja) and by vcpkg
set "CMAKE_BUILD_PARALLEL_LEVEL=%JOBS%"
set "VCPKG_MAX_CONCURRENCY=%JOBS%"
echo Low-load build: %JOBS% parallel jobs (of %NUMBER_OF_PROCESSORS% CPU threads)

if /i "%~1"=="--no-pause" (call "%~dp0build_windows.bat" --no-pause) else (call "%~dp0build_windows.bat")
exit /b %errorlevel%
