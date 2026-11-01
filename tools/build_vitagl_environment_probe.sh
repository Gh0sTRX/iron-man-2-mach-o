#!/usr/bin/env bash
# Compile/link only. No SELF/VPK, original image or native GPU execution.
set -euo pipefail
: "${VITASDK:?Set VITASDK}"
cmake -S vendor/ios-loader-boilerplate -B build/vitagl-environment-probe \
  -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DIM2_LINK_LOADER=ON -DIM2_VITAGL_ENV_PROBE=ON
cmake --build build/vitagl-environment-probe --target im2_vitagl_environment_probe --parallel 2
if arm-vita-eabi-nm -u build/vitagl-environment-probe/im2_vitagl_environment_probe | awk '$1=="U"{print;bad=1} END{exit !bad}'; then
  echo 'FAIL: strong undefined probe ELF symbols' >&2
  exit 1
fi
arm-vita-eabi-objdump -d --disassemble=main build/vitagl-environment-probe/im2_vitagl_environment_probe \
  > build/vitagl-environment-probe/main.disassembly.txt
for symbol in gles_init gles_eagl_ready glReadPixels glDrawArrays; do
  if ! grep -Fq "<$symbol>" build/vitagl-environment-probe/main.disassembly.txt; then
    echo "FAIL: probe entry does not reference $symbol" >&2
    exit 1
  fi
done
sha256sum "$VITASDK/arm-vita-eabi/lib/libvitaGL.a" build/vitagl-environment-probe/im2_vitagl_environment_probe
echo 'PASS: native VitaGL diagnostic ELF linked. NOT executed; GPU/device readiness UNVERIFIED. No SELF/VPK.'
