#!/usr/bin/env bash
# Full research ELF only. Never invokes SELF or VPK packaging.
set -euo pipefail
: "${VITASDK:?Set VITASDK}"
cmake -S vendor/ios-loader-boilerplate -B build/loader-full \
  -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DIM2_LINK_LOADER=ON
cmake --build build/loader-full --parallel 2
arm-vita-eabi-readelf -h build/loader-full/im2_ios_loader
# Optional weak toolchain hooks are permitted. Strong undefined symbols are errors.
if arm-vita-eabi-nm -u build/loader-full/im2_ios_loader | awk '$1=="U"{print;bad=1} END{exit !bad}'; then
  echo 'FAIL: strong undefined ELF symbols' >&2
  exit 1
fi
echo 'PASS: full ARM ELF linked; original imports, original boot and device execution NOT verified'
