#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="build"

echo "=== Building ReconcileX ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j"$(nproc)"

echo "=== Running C++ tests ==="
ctest --output-on-failure -j"$(nproc)"

echo "=== Running Python tests ==="
cd ..
if command -v pytest &>/dev/null; then
    pytest python/tests/ -v
elif [ -d ".venv" ]; then
    .venv/bin/pytest python/tests/ -v
else
    echo "pytest not found, skipping Python tests"
fi

echo "=== All tests passed ==="
