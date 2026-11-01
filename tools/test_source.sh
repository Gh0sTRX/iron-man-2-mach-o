#!/usr/bin/env bash
# All asset-free checks. Owner-image tests require private --ipa/--rbs1 explicitly.
set -euo pipefail
python3 -m compileall -q tools tests
for script in tools/*.sh; do bash -n "$script"; done
for test in check_loader_structure check_objc_dispatch test_objc_sections \
  check_objc_external_relocations test_objc_classref_bindings test_import_audit_synthetic \
  check_native_class_registry test_owner_superclasses_synthetic; do
  python3 "tests/$test.py"
done
bash tools/test_objc_runtime.sh

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I vendor/ios-loader-boilerplate/source tests/test_external_reloc.c \
  vendor/ios-loader-boilerplate/source/macho_reloc.c -o build/host-runtime/test_external_reloc

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer -I vendor/ios-loader-boilerplate/source tests/test_external_reloc_overflow.c vendor/ios-loader-boilerplate/source/macho_reloc.c -o build/host-runtime/test_external_reloc_overflow
build/host-runtime/test_external_reloc_overflow
build/host-runtime/test_external_reloc

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I vendor/ios-loader-boilerplate/source tests/test_initializers.c \
  vendor/ios-loader-boilerplate/source/macho_init.c -o build/host-runtime/test_initializers
build/host-runtime/test_initializers

# The non-PIE host test gives static host class identities 32-bit-representable values;
# guest metadata itself is still read with explicit ARM32 word layouts.
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -no-pie -pthread -I vendor/ios-loader-boilerplate/source tests/test_objc_guest.c \
  vendor/ios-loader-boilerplate/source/objc_guest.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_objc_guest
build/host-runtime/test_objc_guest

python3 tests/test_scanner.py

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_foundation.c \
  vendor/ios-loader-boilerplate/source/foundation.c \
  vendor/ios-loader-boilerplate/source/url.c \
  vendor/ios-loader-boilerplate/source/cfstring.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_foundation

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_data_url.c \
  vendor/ios-loader-boilerplate/source/url.c \
  vendor/ios-loader-boilerplate/source/foundation.c \
  vendor/ios-loader-boilerplate/source/cfstring.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_data_url

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_uikit_hierarchy.c \
  vendor/ios-loader-boilerplate/source/uikit_hierarchy.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_uikit_hierarchy

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_uikit_application.c \
  vendor/ios-loader-boilerplate/source/uikit_application.c \
  vendor/ios-loader-boilerplate/source/uikit_hierarchy.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_uikit_application

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_uikit_screen.c \
  vendor/ios-loader-boilerplate/source/uikit_screen.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -lm -o build/host-runtime/test_uikit_screen

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_eagl_bridge.c \
  vendor/ios-loader-boilerplate/source/eagl_bridge.c \
  vendor/ios-loader-boilerplate/source/numbers.c \
  vendor/ios-loader-boilerplate/source/uikit_hierarchy.c \
  vendor/ios-loader-boilerplate/source/collections.c \
  vendor/ios-loader-boilerplate/source/foundation.c \
  vendor/ios-loader-boilerplate/source/cfstring.c \
  vendor/ios-loader-boilerplate/source/url.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_eagl_bridge
build/host-runtime/test_eagl_bridge
build/host-runtime/test_uikit_screen

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -no-pie -pthread -I vendor/ios-loader-boilerplate/source tests/test_uikit_bootstrap.c \
  vendor/ios-loader-boilerplate/source/uikit_bootstrap.c \
  vendor/ios-loader-boilerplate/source/uikit_application.c \
  vendor/ios-loader-boilerplate/source/uikit_hierarchy.c \
  vendor/ios-loader-boilerplate/source/foundation.c \
  vendor/ios-loader-boilerplate/source/cfstring.c \
  vendor/ios-loader-boilerplate/source/url.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_uikit_bootstrap
build/host-runtime/test_uikit_bootstrap
build/host-runtime/test_uikit_application
build/host-runtime/test_uikit_hierarchy
build/host-runtime/test_data_url
build/host-runtime/test_foundation

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_url.c \
  vendor/ios-loader-boilerplate/source/url.c \
  vendor/ios-loader-boilerplate/source/foundation.c \
  vendor/ios-loader-boilerplate/source/cfstring.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_url

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_url_request.c \
  vendor/ios-loader-boilerplate/source/url_request.c \
  vendor/ios-loader-boilerplate/source/url.c \
  vendor/ios-loader-boilerplate/source/collections.c \
  vendor/ios-loader-boilerplate/source/foundation.c \
  vendor/ios-loader-boilerplate/source/cfstring.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_url_request

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_file_manager.c \
  vendor/ios-loader-boilerplate/source/file_manager.c \
  vendor/ios-loader-boilerplate/source/foundation.c \
  vendor/ios-loader-boilerplate/source/url.c \
  vendor/ios-loader-boilerplate/source/cfstring.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_file_manager
build/host-runtime/test_file_manager
build/host-runtime/test_url_request
build/host-runtime/test_url

python3 tests/test_preflight_synthetic.py

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -no-pie -pthread -I vendor/ios-loader-boilerplate/source tests/test_cfstrings.c \
  vendor/ios-loader-boilerplate/source/foundation.c \
  vendor/ios-loader-boilerplate/source/url.c \
  vendor/ios-loader-boilerplate/source/cfstring.c \
  vendor/ios-loader-boilerplate/source/darwin_printf.c \
  vendor/ios-loader-boilerplate/source/collections.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_cfstrings
build/host-runtime/test_cfstrings

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_collections.c \
  vendor/ios-loader-boilerplate/source/collections.c \
  vendor/ios-loader-boilerplate/source/foundation.c \
  vendor/ios-loader-boilerplate/source/url.c \
  vendor/ios-loader-boilerplate/source/cfstring.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_collections
build/host-runtime/test_collections

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I vendor/ios-loader-boilerplate/source tests/test_gles_oes.c \
  vendor/ios-loader-boilerplate/source/gles_oes.c -o build/host-runtime/test_gles_oes
build/host-runtime/test_gles_oes

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -no-pie -pthread -I vendor/ios-loader-boilerplate/source tests/test_timers.c \
  vendor/ios-loader-boilerplate/source/timers.c \
  vendor/ios-loader-boilerplate/source/uikit_loop.c \
  vendor/ios-loader-boilerplate/source/foundation.c \
  vendor/ios-loader-boilerplate/source/url.c \
  vendor/ios-loader-boilerplate/source/cfstring.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -lm -o build/host-runtime/test_timers
build/host-runtime/test_timers

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_numbers_defaults.c \
  vendor/ios-loader-boilerplate/source/numbers.c \
  vendor/ios-loader-boilerplate/source/defaults.c \
  vendor/ios-loader-boilerplate/source/collections.c \
  vendor/ios-loader-boilerplate/source/foundation.c \
  vendor/ios-loader-boilerplate/source/url.c \
  vendor/ios-loader-boilerplate/source/cfstring.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_numbers_defaults
build/host-runtime/test_numbers_defaults

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_cfbundle.c \
  vendor/ios-loader-boilerplate/source/cfbundle.c \
  vendor/ios-loader-boilerplate/source/imports.c \
  vendor/ios-loader-boilerplate/source/foundation.c \
  vendor/ios-loader-boilerplate/source/url.c \
  vendor/ios-loader-boilerplate/source/cfstring.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_cfbundle
build/host-runtime/test_cfbundle

for test in sets touches; do
  cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -no-pie -pthread -I vendor/ios-loader-boilerplate/source "tests/test_${test}.c" \
    vendor/ios-loader-boilerplate/source/sets.c \
    vendor/ios-loader-boilerplate/source/touches.c \
    vendor/ios-loader-boilerplate/source/uikit_hierarchy.c \
    vendor/ios-loader-boilerplate/source/uikit_application.c \
    vendor/ios-loader-boilerplate/source/numbers.c \
    vendor/ios-loader-boilerplate/source/collections.c \
    vendor/ios-loader-boilerplate/source/foundation.c \
    vendor/ios-loader-boilerplate/source/url.c \
    vendor/ios-loader-boilerplate/source/cfstring.c \
    vendor/ios-loader-boilerplate/source/objc_runtime.c -lm -o "build/host-runtime/test_${test}"
  "build/host-runtime/test_${test}"
done

c++ -std=c++11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I vendor/ios-loader-boilerplate/source tests/test_legacy_string.cpp \
  vendor/ios-loader-boilerplate/source/legacy_string.cpp -o build/host-runtime/test_legacy_string
build/host-runtime/test_legacy_string

c++ -std=c++11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I vendor/ios-loader-boilerplate/source tests/test_legacy_containers.cpp \
  vendor/ios-loader-boilerplate/source/legacy_containers.cpp -o build/host-runtime/test_legacy_containers
build/host-runtime/test_legacy_containers

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_cfarrays.c \
  vendor/ios-loader-boilerplate/source/cfarrays.c vendor/ios-loader-boilerplate/source/cf_memory.c \
  vendor/ios-loader-boilerplate/source/imports.c vendor/ios-loader-boilerplate/source/foundation.c \
  vendor/ios-loader-boilerplate/source/url.c vendor/ios-loader-boilerplate/source/cfstring.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -o build/host-runtime/test_cfarrays
build/host-runtime/test_cfarrays

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I vendor/ios-loader-boilerplate/source tests/test_gles_arrays.c \
  vendor/ios-loader-boilerplate/source/gles_arrays.c -Wl,--wrap=realloc -o build/host-runtime/test_gles_arrays
build/host-runtime/test_gles_arrays

c++ -std=c++11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I vendor/ios-loader-boilerplate/source tests/test_legacy_streams.cpp \
  vendor/ios-loader-boilerplate/source/legacy_streams.cpp -o build/host-runtime/test_legacy_streams
build/host-runtime/test_legacy_streams

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_time_bridge.c \
  vendor/ios-loader-boilerplate/source/time_bridge.c vendor/ios-loader-boilerplate/source/imports.c \
  vendor/ios-loader-boilerplate/source/objc_runtime.c -Wl,--wrap=gettimeofday -lm -o build/host-runtime/test_time_bridge
build/host-runtime/test_time_bridge

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_exit_registry.c \
  vendor/ios-loader-boilerplate/source/exit_registry.c -o build/host-runtime/test_exit_registry
build/host-runtime/test_exit_registry

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -pthread -I vendor/ios-loader-boilerplate/source tests/test_thread_lifecycle.c \
  vendor/ios-loader-boilerplate/source/thread_lifecycle.c \
  -Wl,--wrap=pthread_join,--wrap=pthread_cancel,--wrap=pthread_setcanceltype \
  -o build/host-runtime/test_thread_lifecycle
build/host-runtime/test_thread_lifecycle
