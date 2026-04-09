#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_NAME="kit-linux-build"

echo "Building Docker image: $IMAGE_NAME"
docker build -t "$IMAGE_NAME" "$PROJECT_DIR"
echo "Done. Image ready: $IMAGE_NAME"
