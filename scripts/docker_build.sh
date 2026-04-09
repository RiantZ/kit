#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_NAME="kit-linux-build"

if ! docker image inspect "$IMAGE_NAME" &>/dev/null; then
    echo "Image $IMAGE_NAME not found. Run scripts/docker_create.sh first."
    exit 1
fi

echo "=== Configuring ==="
docker run --rm \
    -v "$PROJECT_DIR:/src:ro" \
    -v kit-build-cache:/build \
    "$IMAGE_NAME" \
    cmake -B /build -S /src

echo "=== Building ==="
docker run --rm \
    -v "$PROJECT_DIR:/src:ro" \
    -v kit-build-cache:/build \
    "$IMAGE_NAME" \
    cmake --build /build

echo "=== Running tests ==="
docker run --rm \
    -v "$PROJECT_DIR:/src:ro" \
    -v kit-build-cache:/build \
    "$IMAGE_NAME" \
    ctest --test-dir /build --output-on-failure

echo "=== All passed ==="
