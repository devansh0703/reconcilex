#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

VENV_DIR=".venv"

echo "=== Setting up Python environment ==="

python3 -m venv "$VENV_DIR"
source "$VENV_DIR/bin/activate"

pip install --upgrade pip
pip install -e ".[dev]"

echo "=== Python setup complete ==="
echo "Activate with: source $VENV_DIR/bin/activate"
