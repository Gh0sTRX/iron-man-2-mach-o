#!/usr/bin/env bash
set -euo pipefail
: "${VITASDK:?Set VITASDK}"
mkdir -p build/arm-dispatch
arm-vita-eabi-gcc -c -marm -ffixed-r9 vendor/ios-loader-boilerplate/source/asm.S -o build/arm-dispatch/asm.o
arm-vita-eabi-gcc -c -marm -ffixed-r9 tests/arm_dispatch_fixture.S -o build/arm-dispatch/fixture.o
arm-vita-eabi-ld -Ttext=0x10000 -e call_guest build/arm-dispatch/asm.o build/arm-dispatch/fixture.o -o build/arm-dispatch/glue.elf
arm-vita-eabi-objcopy -O binary build/arm-dispatch/glue.elf build/arm-dispatch/glue.bin
arm-vita-eabi-nm -n build/arm-dispatch/glue.elf > build/arm-dispatch/symbols.txt
