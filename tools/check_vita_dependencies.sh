#!/usr/bin/env bash
set -euo pipefail
: "${VITASDK:?Set VITASDK to a complete VitaSDK installation}"
for exe in arm-vita-eabi-gcc vita-elf-create vita-make-fself vita-pack-vpk cmake; do
  command -v "$exe" >/dev/null || { echo "MISSING TOOL: $exe" >&2; exit 2; }
done
for file in "$VITASDK/arm-vita-eabi/include/kubridge.h" "$VITASDK/arm-vita-eabi/lib/libkubridge_stub.a"; do
  test -f "$file" || { echo "MISSING KUBRIDGE: $file" >&2; exit 3; }
done
for lib in vitaGL vitashark SceShaccCgExt mathneon SDL2; do
  find "$VITASDK/arm-vita-eabi/lib" -maxdepth 1 \( -name "lib${lib}.a" -o -name "lib${lib}.so" \) | grep -q . || { echo "MISSING LIBRARY: $lib" >&2; exit 4; }
done
echo "VitaSDK tools, kubridge and graphics library files detected"
echo "Note: this does not validate loader compilation or runtime plugin installation"
