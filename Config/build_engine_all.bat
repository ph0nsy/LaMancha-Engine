@echo off
REM Builds LaMancha Engine for ALL platforms from Windows

setlocal enabledelayedexpansion

echo * LaMancha Engine - Universal Multi-Platform Build *
echo ====================================================
echo.
echo Building from: Windows
echo Target platforms: Windows, Linux, R36S
echo.

REM Check if we're in the right directory
if not exist "EngineSource" (
    echo ERROR: Must run from LaManchaEngine root directory
    pause
    exit /b 1
)

set /A BUILD_FAILED=0

REM > Build 1: Native Windows (x86-64) <
REM ====================================

echo [1/3] - Building for Windows (native)
echo =====================================
echo.

if not exist "build-windows" mkdir build-windows
cd build-windows

cmake .. ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DTARGET_PLATFORM=windows ^
    -DLAMANCHA_BUILD_ENGINE=ON ^
    -DLAMANCHA_BUILD_TOOLS=OFF ^
    -DLAMANCHA_BUILD_PROJECTS=OFF

if %ERRORLEVEL% neq 0 (
    echo ERROR: Windows CMake configuration failed
    set /A BUILD_FAILED=BUILD_FAILED+1
    cd ..
    goto :linux_build
)

cmake --build . --config Release -j 8

if %ERRORLEVEL% neq 0 (
    echo ERROR: Windows build failed
    set /A BUILD_FAILED=BUILD_FAILED+1
)

cd ..

:linux_build

REM > Build 2: Linux (cross-compile via WSL2 or skip) <
REM ===================================================

echo.
echo [2/3] - Building for Linux (cross-compile)
echo ==========================================
echo.

REM Check if WSL2 is available
where wsl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo WSL2 detected, using for Linux build...
    
    REM Run build inside WSL2
    wsl bash -c "cd '$(wslpath '%CD%')' && ./scripts/build_engine_linux.sh"
    
    if %ERRORLEVEL% neq 0 (
        echo ERROR: Linux build failed
        set /A BUILD_FAILED=BUILD_FAILED+1
    )
) else (
    echo WARNING: WSL2 not found, skipping Linux build
    echo.
    echo To build for Linux from Windows:
    echo   1. Install WSL2: wsl --install
    echo   2. Install Ubuntu: wsl --install -d Ubuntu
    echo   3. Install build tools: wsl sudo apt install build-essential cmake
    echo.
)

REM > Build 3: R36S (cross-compile with ARM toolchain) <
REM ====================================================

echo.
echo [3/3] - Building for R36S (ARM64)
echo =================================
echo.

REM Check if ARM toolchain exists
if not exist "cmake\r36s-toolchain.cmake" (
    echo WARNING: R36S toolchain not found at cmake\r36s-toolchain.cmake
    echo Skipping R36S build
    echo.
    echo To enable R36S builds:
    echo   1. Download R36S SDK
    echo   2. Create cmake\r36s-toolchain.cmake
    echo.
    goto :summary
)

REM Check if ARM compiler exists
set ARM_COMPILER_PATH=C:\arm-toolchain\bin\aarch64-linux-gnu-gcc.exe
if not exist "%ARM_COMPILER_PATH%" (
    echo WARNING: ARM compiler not found at %ARM_COMPILER_PATH%
    echo Checking WSL2 for ARM toolchain...
    
    where wsl >nul 2>nul
    if %ERRORLEVEL% equ 0 (
        echo Using WSL2 for R36S cross-compile...
        wsl bash -c "cd '$(wslpath '%CD%')' && ./scripts/build_engine_r36s.sh"
        
        if %ERRORLEVEL% neq 0 (
            echo ERROR: R36S build failed
            set /A BUILD_FAILED=BUILD_FAILED+1
        )
    ) else (
        echo WARNING: Cannot build R36S without ARM toolchain or WSL2
    )
    goto :summary
)

REM Build with native Windows ARM toolchain
if not exist "build-r36s" mkdir build-r36s
cd build-r36s

cmake .. ^
    -G "MinGW Makefiles" ^
    -DCMAKE_TOOLCHAIN_FILE=..\cmake\r36s-toolchain.cmake ^
    -DTARGET_PLATFORM=r36s ^
    -DLAMANCHA_BUILD_ENGINE=ON ^
    -DLAMANCHA_BUILD_TOOLS=OFF ^
    -DLAMANCHA_BUILD_PROJECTS=OFF

if %ERRORLEVEL% neq 0 (
    echo ERROR: R36S CMake configuration failed
    set /A BUILD_FAILED=BUILD_FAILED+1
    cd ..
    goto :summary
)

cmake --build . --config Release -j 8

if %ERRORLEVEL% neq 0 (
    echo ERROR: R36S build failed
    set /A BUILD_FAILED=BUILD_FAILED+1
)

cd ..

:summary

REM > Build Summary <
REM =================

echo.
echo * Build Summary *
echo =================
echo.

if exist "build-windows\lib\LaManchaEngine.lib" (
    echo [OK] Windows: build-windows\lib\LaManchaEngine.lib
) else (
    echo [FAIL] Windows build not found
)

if exist "build-linux\lib\LaManchaEngine.a" (
    echo [OK] Linux:   build-linux\lib\LaManchaEngine.a
) else (
    echo [SKIP] Linux build not found
)

if exist "build-r36s\lib\LaManchaEngine.a" (
    echo [OK] R36S:    build-r36s\lib\LaManchaEngine.a
) else (
    echo [SKIP] R36S build not found
)

echo.

if %BUILD_FAILED% equ 0 (
    echo ================================================
    echo.
    echo All available platforms built successfully!
    echo.
    echo ================================================
) else (
    echo ================================================
    echo.
    echo [%BUILD_FAILED%/3] builds failed! 
    echo See errors above
    echo.
    echo ================================================
)

pause