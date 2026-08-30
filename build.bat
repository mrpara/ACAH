@echo off
rem ===========================================================================
rem  SpiderBot - command line build. No IDE required.
rem
rem    build.bat            configure (if needed) and build Release
rem    build.bat run        build, then launch the game
rem    build.bat debug      build a Debug configuration
rem    build.bat headless   build and run the windowless render test
rem    build.bat clean      delete the build directory
rem
rem  Needs: CMake 3.16+ and a C++17 compiler on PATH.
rem  See README.md for the one-time toolchain install.
rem ===========================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "MODE=%~1"
if "%MODE%"=="" set "MODE=build"

if /i "%MODE%"=="clean" (
    if exist build rmdir /s /q build
    echo Removed build directory.
    goto :eof
)

set "CONFIG=Release"
if /i "%MODE%"=="debug" set "CONFIG=Debug"

rem --- locate cmake -----------------------------------------------------------
where cmake >nul 2>nul
if errorlevel 1 (
    echo.
    echo   ERROR: cmake was not found on PATH.
    echo   Install it with:  winget install Kitware.CMake
    echo   then open a NEW terminal and run this script again.
    echo.
    exit /b 1
)

rem --- make sure a compiler is visible ---------------------------------------
rem MSVC only puts cl.exe on PATH inside a Developer Command Prompt. If we are
rem not in one, find the Build Tools installation with vswhere and load its
rem environment ourselves, so a plain terminal works too.
where cl >nul 2>nul
if not errorlevel 1 goto :have_compiler

where g++ >nul 2>nul
if not errorlevel 1 (
    set "GENERATOR=-G "MinGW Makefiles""
    goto :have_compiler
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo.
    echo   ERROR: no C++ compiler found.
    echo   Install the Visual Studio Build Tools with:
    echo       winget install Microsoft.VisualStudio.2022.BuildTools --override "--quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    echo   then open a NEW terminal and run this script again.
    echo.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo   ERROR: Visual Studio C++ tools are installed but incomplete.
    echo   Re-run the Build Tools installer and include "Desktop development with C++".
    exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo   ERROR: failed to initialise the MSVC environment.
    exit /b 1
)

:have_compiler

rem --- configure --------------------------------------------------------------
if not exist build\CMakeCache.txt (
    echo Configuring ^(%CONFIG%^)...
    echo The first configure downloads and builds SDL3; expect a few minutes.
    cmake -B build -S . -DCMAKE_BUILD_TYPE=%CONFIG% %GENERATOR%
    if errorlevel 1 (
        echo.
        echo   Configure failed. If the SDL3 download timed out, check your
        echo   network and run this script again - it resumes from where it stopped.
        exit /b 1
    )
)

rem --- build ------------------------------------------------------------------
echo Building %CONFIG%...
cmake --build build --config %CONFIG% --parallel
if errorlevel 1 (
    echo.
    echo   Build failed. See the errors above.
    exit /b 1
)

set "EXE=build\bin\spiderbot.exe"
if not exist "%EXE%" set "EXE=build\bin\%CONFIG%\spiderbot.exe"
if not exist "%EXE%" set "EXE=build\%CONFIG%\spiderbot.exe"

echo.
echo Built: %EXE%
echo.

if /i "%MODE%"=="run" (
    "%EXE%" %2 %3 %4 %5 %6 %7 %8 %9
    goto :eof
)

if /i "%MODE%"=="headless" (
    set "HEXE=build\bin\spiderbot_headless.exe"
    if not exist "!HEXE!" set "HEXE=build\bin\%CONFIG%\spiderbot_headless.exe"
    if not exist "out" mkdir out
    "!HEXE!" --frames 240 --cols 200 --rows 56 --out out --hist
    echo.
    echo Wrote frame captures to the out\ directory ^(.ppm images and .txt dumps^).
    goto :eof
)

echo Run it with:  build.bat run
