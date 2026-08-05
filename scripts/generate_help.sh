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

# LC_ALL=C forces byte-order (ASCII) string comparison in awk. Under a UTF-8
# locale awk collates punctuation loosely (e.g. ";" sorts before "."), which
# breaks the ascending-strcasecmp invariant that help_lookup's binary search
# relies on.
LC_ALL=C awk -f "${SCRIPT_DIR}/generate_help.awk" "$INPUT" > "$OUTPUT"
