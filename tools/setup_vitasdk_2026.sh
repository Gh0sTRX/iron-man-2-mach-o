#!/usr/bin/env bash
set -euo pipefail
export VITASDK="${VITASDK:-$HOME/vitasdk}"
export PATH="$VITASDK/bin:$PATH"
if ! command -v arm-vita-eabi-gcc >/dev/null 2>&1; then
  if [ -e "$VITASDK" ]; then
    echo "Existing incomplete VitaSDK directory: $VITASDK; refusing to overwrite" >&2
    exit 2
  fi
  tempdir="$(mktemp -d)"
  trap 'rm -rf "$tempdir"' EXIT
  git clone --depth 1 https://github.com/vitasdk/vdpm "$tempdir/vdpm"
  (cd "$tempdir/vdpm" && VITASDK_CHANNEL=2026.08 ./bootstrap-vitasdk.sh)
fi
for tool in arm-vita-eabi-gcc vita-elf-create vita-make-fself vita-pack-vpk; do
  command -v "$tool" || { echo "Missing tool: $tool" >&2; exit 3; }
done
test -f "$VITASDK/share/vita.toolchain.cmake"
test -f "$VITASDK/share/vita.cmake"
vdpm status
for pkg in kubridge vitaGL; do
  echo "Checking optional library $pkg"
  vdpm info "$pkg" || true
done
echo "VitaSDK core verified. Install optional packages only as build errors require."
