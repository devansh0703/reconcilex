#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="build"
BUILD_TYPE="${1:-Release}"

echo "=== Building ReconcileX (${BUILD_TYPE}) ==="

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build . -j"$(nproc)"

echo "=== Build complete ==="
echo "Binary: $BUILD_DIR/reconcilex"
