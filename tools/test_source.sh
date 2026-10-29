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
