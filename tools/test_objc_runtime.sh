#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo"
mkdir -p build/host-runtime
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -pthread -I vendor/ios-loader-boilerplate/source \
  tests/test_objc_runtime.c vendor/ios-loader-boilerplate/source/objc_runtime.c \
  -o build/host-runtime/test_objc_runtime
build/host-runtime/test_objc_runtime
