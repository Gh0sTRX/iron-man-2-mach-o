#!/usr/bin/env bash
# Compile only; default CI does not create diagnostic packages.
set -euo pipefail
: "${VITASDK:?Set VITASDK}"
cmake -S tests/vita_exec_probe -B build/vm-environment-probe \
  -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DIM2_PACKAGE_VM_PROBE=OFF
cmake --build build/vm-environment-probe --target im2_exec_probe --parallel 2
if arm-vita-eabi-nm -u build/vm-environment-probe/im2_exec_probe | awk '$1=="U"{print;bad=1} END{exit !bad}'; then
  echo 'FAIL: unresolved strong VM diagnostic symbols' >&2
  exit 1
fi
echo 'PASS: VM-domain diagnostic ELF compiled/linked only. Native VM/RX/cache NOT tested.'
