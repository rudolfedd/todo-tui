#!/usr/bin/env bash
# build-msys2.sh — build todo-tui on Windows with MSYS2 / mingw-w64 + PDCurses.
#
# Usage (from an MSYS2 "MSYS2 UCRT64" or "MINGW64" shell):
#   ./build-msys2.sh
#
# One-off setup:
#   pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-pdcurses make

set -euo pipefail

cd "$(dirname "$0")"

case "${MSYSTEM:-}" in
  MINGW64|UCRT64|CLANG64|CLANGARM64) ;;
  *)
    echo "warning: \$MSYSTEM is '${MSYSTEM:-unset}' — start an MSYS2 MINGW64/UCRT64 shell" >&2
    ;;
esac

CC="${CC:-gcc}"
CFLAGS="-std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow"
LDFLAGS=""
LDLIBS="-lpdcurses"

if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists pdcurses; then
  CFLAGS="$(pkg-config --cflags pdcurses) $CFLAGS"
  LDLIBS="$(pkg-config --libs pdcurses)"
fi

mkdir -p build
OBJS=()
for src in src/*.c; do
  obj="build/$(basename "${src%.c}").o"
  echo "  CC  $src"
  # shellcheck disable=SC2086
  $CC $CFLAGS -c "$src" -o "$obj"
  OBJS+=("$obj")
done

echo "  LD  todo-tui.exe"
# shellcheck disable=SC2086
$CC $CFLAGS $LDFLAGS -o todo-tui.exe "${OBJS[@]}" $LDLIBS

echo
echo "Built: $(pwd)/todo-tui.exe"
echo "Run  : ./todo-tui.exe"
