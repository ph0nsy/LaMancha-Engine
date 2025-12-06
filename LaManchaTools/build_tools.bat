@echo off
REM Builds all LaMancha development tools

setlocal enabledelayedexpansion

echo * LaMancha Tools - Build Script (Windows) *
echo ===========================================
echo.

REM Check if we're in the right directory
if not exist "LaManchaTools" (
    echo ERROR: Must run from LaManchaEngine root directory
    pause
    exit /b 1
)

REM Check if engine is built
if not exist "build-windows\lib\LaManchaEngine.lib" (
    echo ERROR: Engine not built for Windows
    echo Please run scripts\build_engine_windows.bat first
    pause
    exit /b 1
)

cd LaManchaTools

REM Configure with CMake
echo.
echo Configuring tools with CMake...
cmake .. ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DLAMANCHA_BUILD_ENGINE=OFF ^
    -DLAMANCHA_BUILD_TOOLS=ON ^
    -DLAMANCHA_BUILD_PROJECTS=OFF ^
    -DLaManchaEngine_DIR="%CD%\..\build-windows"

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: CMake configuration failed
    cd ..
    pause
    exit /b 1
)

REM Build tools
echo.
echo Building tools...
cmake --build . --config Release -j 8

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Build failed
    cd ..
    pause
    exit /b 1
)

echo.
echo ================================================
echo Tools built successfully!
echo ================================================
echo.
echo Available tools:
echo   - LaManchaTools\bin\LaManchaTools.exe
echo   - LaManchaTools\bin\LaManchaProjectCreator.exe [Upcoming]
echo   - LaManchaTools\bin\LaManchaProfiler.exe [Upcoming]

REM Translates from scene built with assets in custom editor to seq_<name>.ini
echo   - LaManchaTools\bin\LaManchaCinematicSequence.exe [Upcoming]

REM Translates from MIDI editor into midi_<name>.lua 
echo   - LaManchaTools\bin\LaManchaMusicSequence.exe [Upcoming]
echo.

REM Copy tools to convenient location
if not exist "bin" mkdir bin
copy /Y bin\*.exe bin\ >nul 2>&1

echo Tools copied to bin\
echo.
pause