#!/bin/sh
# generate_help.sh — wrapper for CMake to invoke generate_help.awk
# Usage: generate_help.sh <input.md> <output.c>

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INPUT="$1"
OUTPUT="$2"

if [ -z "$INPUT" ] || [ -z "$OUTPUT" ]; then
    echo "Usage: $0 <input.md> <output.c>" >&2
    exit 1
fi

awk -f "${SCRIPT_DIR}/generate_help.awk" "$INPUT" > "$OUTPUT"
