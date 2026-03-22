#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(CDPATH= cd "$SCRIPT_DIR/.." && pwd)"
COMMAND="${1:-build}"

cd "$ROOT_DIR"
cc -std=c99 -Wall -Wextra nob.c -o nob
./nob "$COMMAND"
