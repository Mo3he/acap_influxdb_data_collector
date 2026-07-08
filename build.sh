#!/bin/sh
# Usage: ./build.sh [aarch64|armv7hf ...]   (default: both)
# Override the container runtime with RUNTIME=docker|podman.
set -e

# Honor an explicit RUNTIME override; otherwise use docker if available and fall
# back to podman (drop-in compatible).
if [ -z "${RUNTIME:-}" ]; then
    if command -v docker >/dev/null 2>&1; then
        RUNTIME=docker
    elif command -v podman >/dev/null 2>&1; then
        RUNTIME=podman
    else
        echo "Error: neither docker nor podman found in PATH" >&2
        exit 1
    fi
fi
echo "=== Using container runtime: $RUNTIME ==="

rm -rf build *.eap

build_arch() {
    ARCH=$1
    echo "=== Building $ARCH ==="
    $RUNTIME build --progress=plain --no-cache --build-arg ARCH="$ARCH" --tag "acap_influxdb_$ARCH" .
    CONTAINER_ID=$($RUNTIME create "acap_influxdb_$ARCH")
    $RUNTIME cp "$CONTAINER_ID":/opt/app ./build
    $RUNTIME rm "$CONTAINER_ID"
    mv build/*.eap .
    rm -rf build
}

if [ "$#" -gt 0 ]; then
    for a in "$@"; do build_arch "$a"; done
else
    build_arch aarch64
    build_arch armv7hf
fi

echo "=== Done ==="
ls -lh *.eap
