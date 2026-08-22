#!/usr/bin/env sh
set -eu

mkdir -p dist
x86_64-w64-mingw32-windres native/desktop_switch_guard.rc -O coff -o dist/desktop_switch_guard.res
x86_64-w64-mingw32-gcc \
  -std=c17 \
  -O2 \
  -Wall \
  -Wextra \
  -Werror \
  -municode \
  -mwindows \
  native/desktop_switch_guard.c \
  dist/desktop_switch_guard.res \
  -o dist/DesktopSwitchGuard.exe \
  -luser32 \
  -lshell32
sha256sum dist/DesktopSwitchGuard.exe > CHECKSUMS.txt
rm -f dist/desktop_switch_guard.res
