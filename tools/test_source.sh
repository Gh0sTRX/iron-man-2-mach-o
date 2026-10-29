#!/usr/bin/env bash
# All asset-free checks. Owner-image tests require private --ipa/--rbs1 explicitly.
set -euo pipefail
python3 -m compileall -q tools tests
for script in tools/*.sh; do bash -n "$script"; done
for test in check_loader_structure check_objc_dispatch test_objc_sections \
  check_objc_external_relocations test_objc_classref_bindings test_import_audit_synthetic; do
  python3 "tests/$test.py"
done
bash tools/test_objc_runtime.sh
