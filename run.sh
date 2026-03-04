#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo "==> Configuring project..."
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR"

echo "==> Building project..."
cmake --build "$BUILD_DIR" -j

echo "==> Running app..."
exec "$BUILD_DIR/PlushEngine"
