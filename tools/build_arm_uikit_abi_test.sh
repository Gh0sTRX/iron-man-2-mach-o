#!/usr/bin/env bash
# Execute actual production UIKit CGRect IMPs without SDK/OS backends.
set -euo pipefail
: "${VITASDK:?Set VITASDK}"
mkdir -p build/arm-uikit
for source in vendor/ios-loader-boilerplate/source/uikit_hierarchy.c tests/arm_uikit_abi_fixture.c; do
  output="build/arm-uikit/$(basename "$source").o"
  arm-vita-eabi-gcc -std=c11 -O2 -marm -mfpu=vfpv3-d16 -mfloat-abi=hard -ffixed-r9 \
    -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables \
    -I vendor/ios-loader-boilerplate/source -c "$source" -o "$output"
done
arm-vita-eabi-ld -T tests/arm_uikit_abi.ld build/arm-uikit/*.o \
  build/arm-dispatch/asm.o build/arm-dispatch/fixture.o -o build/arm-uikit/uikit.elf
arm-vita-eabi-objcopy -O binary build/arm-uikit/uikit.elf build/arm-uikit/uikit.bin
arm-vita-eabi-nm -n build/arm-uikit/uikit.elf > build/arm-uikit/symbols.txt
