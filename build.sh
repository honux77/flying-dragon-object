#!/bin/sh
# Build wbml-ref on macOS / Linux.
# Prerequisites (macOS): brew install sdl2
# Prerequisites (Linux): sudo apt install libsdl2-dev cmake build-essential
#
# Usage:
#   ./build.sh           — Release build
#   ./build.sh --debug   — Debug build (console output visible)
set -e

BUILD_TYPE=Release
for arg in "$@"; do
    case "$arg" in
        --debug) BUILD_TYPE=Debug ;;
    esac
done

cmake -B cmake-build -S . -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build cmake-build --parallel

echo ""
echo "Built: build/wbml-ref"
echo "Run from project root: ./build/wbml-ref"
