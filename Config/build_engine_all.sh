#!/bin/bash
# Builds LaMancha Engine for ALL platforms from Linux

set -e  # Exit on error (disabled for optional builds)

echo "> LaMancha Engine - Multi-Platform Build <"
echo "=========================================="
echo ""
echo "Building from: Linux"
echo "Target platforms: Windows, Linux, R36S"
echo ""

# Check if we're in the right directory
if [ ! -d "EngineSource" ]; then
    echo "ERROR: Must run from LaManchaEngine root directory"
    exit 1
fi

BUILD_FAILED=0

# > Build 1: Native Linux (x86-64) <
# ==================================

echo "[1/3] - Building for Linux (native)"
echo "==================================="
echo ""

mkdir -p build-linux
cd build-linux

cmake .. \
    -DTARGET_PLATFORM=linux \
    -DLAMANCHA_BUILD_ENGINE=ON \
    -DLAMANCHA_BUILD_TOOLS=OFF \
    -DLAMANCHA_BUILD_PROJECTS=OFF \
    -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "ERROR: Linux CMake configuration failed"
    ((BUILD_FAILED++))
    cd ..
else
    cmake --build . --config Release -j $(nproc)
    
    if [ $? -ne 0 ]; then
        echo "ERROR: Linux build failed"
        ((BUILD_FAILED++))
    fi
    cd ..
fi

# > Build 2: Windows (cross-compile with MinGW-w64) <
# ===================================================

echo ""
echo "[2/3] - Building for Windows (cross-compile)"
echo "============================================"
echo ""

# Check if MinGW-w64 is installed
if command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "MinGW-w64 detected, cross-compiling for Windows..."
    
    mkdir -p build-windows
    cd build-windows
    
    cmake .. \
        -DCMAKE_TOOLCHAIN_FILE=../cmake/windows-from-linux.cmake \
        -DTARGET_PLATFORM=windows \
        -DLAMANCHA_BUILD_ENGINE=ON \
        -DLAMANCHA_BUILD_TOOLS=OFF \
        -DLAMANCHA_BUILD_PROJECTS=OFF \
        -DCMAKE_BUILD_TYPE=Release
    
    if [ $? -ne 0 ]; then
        echo "ERROR: Windows CMake configuration failed"
        ((BUILD_FAILED++))
        cd ..
    else
        cmake --build . --config Release -j $(nproc)
        
        if [ $? -ne 0 ]; then
            echo "ERROR: Windows build failed"
            ((BUILD_FAILED++))
        fi
        cd ..
    fi
else
    echo "WARNING: MinGW-w64 not found, skipping Windows build"
    echo ""
    echo "To build for Windows from Linux:"
    echo "  sudo apt install mingw-w64 g++-mingw-w64-x86-64"
    echo ""
fi

# > Build 3: R36S (cross-compile with ARM toolchain) <
# ====================================================

echo ""
echo "[3/3] - Building for R36S (ARM64)"
echo "================================="
echo ""

# Check if R36S toolchain exists
if [ ! -f "cmake/r36s-toolchain.cmake" ]; then
    echo "WARNING: R36S toolchain not found at cmake/r36s-toolchain.cmake"
    echo "Skipping R36S build"
    echo ""
    echo "To enable R36S builds:"
    echo "  1. Download R36S SDK"
    echo "  2. Create cmake/r36s-toolchain.cmake"
    echo ""
else
    # Check if ARM compiler exists
    if command -v aarch64-linux-gnu-gcc &> /dev/null; then
        echo "ARM toolchain detected, cross-compiling for R36S..."
        
        mkdir -p build-r36s
        cd build-r36s
        
        cmake .. \
            -DCMAKE_TOOLCHAIN_FILE=../cmake/r36s-toolchain.cmake \
            -DTARGET_PLATFORM=r36s \
            -DLAMANCHA_BUILD_ENGINE=ON \
            -DLAMANCHA_BUILD_TOOLS=OFF \
            -DLAMANCHA_BUILD_PROJECTS=OFF \
            -DCMAKE_BUILD_TYPE=Release
        
        if [ $? -ne 0 ]; then
            echo "ERROR: R36S CMake configuration failed"
            ((BUILD_FAILED++))
            cd ..
        else
            cmake --build . --config Release -j $(nproc)
            
            if [ $? -ne 0 ]; then
                echo "ERROR: R36S build failed"
                ((BUILD_FAILED++))
            fi
            cd ..
        fi
    else
        echo "WARNING: ARM toolchain not found"
        echo ""
        echo "To install ARM cross-compiler:"
        echo "  sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
        echo ""
    fi
fi

# > Build Summary <
# =================

echo ""
echo "> Build Summary <"
echo "================="
echo ""

if [ -f "build-linux/lib/LaManchaEngine.a" ]; then
    echo "[OK] Linux:   build-linux/lib/LaManchaEngine.a"
else
    echo "[FAIL] Linux build not found"
fi

if [ -f "build-windows/lib/LaManchaEngine.lib" ] || [ -f "build-windows/lib/libLaManchaEngine.a" ]; then
    echo "[OK] Windows: build-windows/lib/"
else
    echo "[SKIP] Windows build not found"
fi

if [ -f "build-r36s/lib/LaManchaEngine.a" ]; then
    echo "[OK] R36S:    build-r36s/lib/LaManchaEngine.a"
else
    echo "[SKIP] R36S build not found"
fi

echo ""

if [ $BUILD_FAILED -eq 0 ]; then
    echo "================================================"
    echo ""
    echo "All available platforms built successfully!"
    echo ""
    echo "================================================"
else
    echo "================================================"
    echo ""
    echo "[$BUILD_FAILED/3] builds failed"
    echo "See errors above"
    echo ""
    echo "================================================"
    exit 1
fi