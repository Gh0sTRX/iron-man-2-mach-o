#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/host-runtime
cc -std=c11 -Wall -Wextra -Werror -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie \
  -I tests/host_vita -I vendor/ios-loader-boilerplate/source \
  tests/test_macho_load_bounds.c vendor/ios-loader-boilerplate/source/macho_reloc.c \
  vendor/ios-loader-boilerplate/source/macho_init.c -Wl,--wrap=malloc -o build/host-runtime/test_macho_load_bounds
build/host-runtime/test_macho_load_bounds
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  tests/test_vm_probe_code.c -o build/host-runtime/test_vm_probe_code
build/host-runtime/test_vm_probe_code
