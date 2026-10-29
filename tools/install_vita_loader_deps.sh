#!/usr/bin/env bash
# Install *build-time* dependencies into an existing VitaSDK 2026.08 environment.
# Does NOT install kernel plugins on the handheld or package a VPK.
set -euo pipefail
: "${VITASDK:?Install VitaSDK first and set VITASDK}"
command -v vdpm >/dev/null || { echo "vdpm not found in PATH" >&2; exit 2; }
vdpm install \
  kubridge \
  vitaGL \
  vitaShaRK \
  SceShaccCgExt \
  taihen \
  freetype \
  libpng \
  zlib \
  bzip2 \
  libmathneon \
  sdl2
echo "Build-time VitaSDK dependency transaction completed."
echo "Runtime kubridge.skprx and libshacccg.suprx are separate on-device requirements."
