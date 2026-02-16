#!/bin/sh
set -e

PRESET="pico+2w"
BUILD_DIR="build-$PRESET"
ELF="$BUILD_DIR/pico-logo.elf"

OPENOCD="$HOME/.pico-sdk/openocd/0.12.0+dev/openocd.exe"
OPENOCD_SCRIPTS="$HOME/.pico-sdk/openocd/0.12.0+dev/scripts"

echo "=== Building $PRESET ==="
cmake --preset="$PRESET"
cmake --build --preset="$PRESET"

echo "=== Flashing $ELF via SWD ==="
"$OPENOCD" -s "$OPENOCD_SCRIPTS" \
    -f interface/cmsis-dap.cfg \
    -f target/rp2350.cfg \
    -c "adapter speed 5000" \
    -c "program $ELF verify reset exit"

echo "=== Done ==="
