#!/bin/bash
# scripts/build_all_tools.sh
# Builds all LaMancha development tools

set -e

echo "> LaMancha Tools - Build Script (Linux/Mac) <"
echo "============================================="
echo ""

# Check if we're in the right directory
if [ ! -d "LaManchaTools" ]; then
    echo "ERROR: Must run from LaManchaEngine root directory"
    exit 1
fi

# Detect platform
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos" # in case there is support in the future
else
    echo "ERROR: Unknown platform: $OSTYPE"
    exit 1
fi

# Check if engine is built
if [ ! -f "build-$PLATFORM/lib/LaManchaEngine.a" ]; then
    echo "ERROR: Engine not built for $PLATFORM"
    echo "Please run scripts/build_engine_$PLATFORM.sh first"
    exit 1
fi

# Create tools build directory
cd LaManchaTools

# Configure with CMake
echo ""
echo "Configuring tools with CMake..."
cmake .. \
    -DLAMANCHA_BUILD_ENGINE=OFF \
    -DLAMANCHA_BUILD_TOOLS=ON \
    -DLAMANCHA_BUILD_PROJECTS=OFF \
    -DLaManchaEngine_DIR="$(pwd)/../build-$PLATFORM" \
    -DCMAKE_BUILD_TYPE=Release

# Build tools
echo ""
echo "Building tools..."
cmake --build . --config Release -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)

echo ""
echo "================================================"
echo "Tools built successfully!"
echo "================================================"
echo ""
echo "Available tools:"
echo "  - LaManchaTools/bin/LaManchaTools"
echo "  - LaManchaTools/bin/LaManchaProjectCreator [Upcoming]"
echo "  - LaManchaTools/bin/LaManchaProfiler [Upcoming]"
echo "  - LaManchaTools/bin/LaManchaCinematicSequence [Upcoming]" # Translates from scene built with assets to seq_<name>.ini
echo "  - LaManchaTools/bin/LaManchaMusicSequence [Upcoming]" # Translates from MIDI editor into midi_<name>.lua
echo ""

# Copy tools to convenient location
mkdir -p bin
cp bin/LaMancha* bin/ 2>/dev/null || true

echo "Tools copied to bin/"
echo ""