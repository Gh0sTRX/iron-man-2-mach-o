#!/usr/bin/env bash
# Explicit opt-in asset-free packages. Never called by default CI or game release.
set -euo pipefail
: "${VITASDK:?Set VITASDK}"
cmake -S tests/device_diagnostics -B build/device-diagnostics \
  -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/device-diagnostics --target im2-vitagl-diagnostic.vpk-vpk im2-vm-diagnostic.vpk-vpk --parallel 2
python3 tests/check_device_packages.py build/device-diagnostics
sha256sum build/device-diagnostics/im2-vitagl-diagnostic.vpk build/device-diagnostics/im2-vm-diagnostic.vpk
echo 'Built diagnostic packages only. Physical GPU/VM/RX/cache execution NOT verified.'
