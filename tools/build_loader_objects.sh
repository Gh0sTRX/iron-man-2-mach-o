#!/usr/bin/env bash
# Usage: bash tools/build_loader_objects.sh
# No ELF, SELF, eboot.bin or VPK is produced.
set -euo pipefail
: "${VITASDK:?Set VITASDK to your VitaSDK installation}"
command -v cmake >/dev/null
command -v arm-vita-eabi-gcc >/dev/null
cmake -S vendor/ios-loader-boilerplate -B build/loader-objects \
  -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/loader-objects --parallel 2
echo "PASS: Loader source objects compiled for ARM VitaSDK"
echo "NOT VERIFIED: ELF link, missing symbols, runtime plugins, guest executable memory, game boot"
