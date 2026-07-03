#!/bin/sh
# End-to-end golden-file tests for the host REPL.
#
# Pipes each tests/e2e/*.logo script through the host `logo` binary and
# diffs the combined output against the matching *.expected file.
#
# Usage: tests/e2e/run_e2e.sh [path-to-logo-binary]
#        (default: build-host/logo, relative to the repo root)
set -u

dir=$(dirname "$0")
logo=${1:-"$dir/../../build-host/logo"}

if [ ! -x "$logo" ]; then
    echo "error: logo binary not found at $logo" >&2
    echo "build it with: cmake --preset=host && cmake --build --preset=host" >&2
    exit 2
fi

fail=0
for script in "$dir"/*.logo; do
    name=$(basename "$script" .logo)
    expected="$dir/$name.expected"
    if [ ! -f "$expected" ]; then
        echo "SKIP $name (no .expected file)"
        continue
    fi
    actual=$("$logo" < "$script" 2>&1)
    status=$?
    if [ $status -ne 0 ]; then
        echo "FAIL $name (exit $status)"
        fail=1
        continue
    fi
    if [ "$actual" = "$(cat "$expected")" ]; then
        echo "PASS $name"
    else
        echo "FAIL $name"
        # printf, not echo: echo may mangle backslashes or a leading -n
        printf '%s\n' "$actual" | diff -u "$expected" - | sed 's/^/    /'
        fail=1
    fi
done
exit $fail
