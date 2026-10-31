#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
command -v cmake >/dev/null || { echo 'CMake is missing. Install it first: brew install cmake'; exit 1; }
architecture="${CARIS_ARCH:-$(uname -m)}"
echo "Building Caris Throw Matrix for macOS architecture: $architecture"
if [[ -f build/CMakeCache.txt ]] && grep -q "CMAKE_GENERATOR:INTERNAL=Xcode" build/CMakeCache.txt 2>/dev/null; then
    mv build build-xcode-old
fi
if (( $# > 0 )); then
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$architecture" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "$@"
else
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$architecture" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
fi
cmake --build build --config Release --target CarisThrowMatrix_AU CarisThrowMatrix_VST3 --parallel 4
echo
printf '%s\n' 'Generated formats:'
find build -type d \( -name '*.component' -o -name '*.vst3' \) -print
printf '%s\n' 'To install locally, use: ./install-macos.sh'
