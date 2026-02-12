#!/bin/sh
set -e

PRESETS="pico pico2 pico2w"

mkdir -p dist

for preset in $PRESETS; do
    echo "=== Building $preset ==="
    cmake --preset="$preset"
    cmake --build --preset="$preset"
    cp "build-$preset/pico-logo.uf2" "dist/logo-$preset.uf2"
    echo "=== Built dist/logo-$preset.uf2 ==="
done

echo "=== Creating logo.zip ==="
(cd logo && zip -r ../dist/logo.zip .)
echo "=== Created dist/logo.zip ==="

echo ""
echo "All builds complete:"
ls -lh dist/logo-*.uf2 dist/logo.zip
