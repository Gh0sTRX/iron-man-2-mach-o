#!/usr/bin/env python3
"""Scope guard, not a GPU test. Never executes or models the native probe."""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
probe=(root/'tests/vitagl_environment_probe/main.c').read_text()
code=re.sub(r'/\*.*?\*/|//[^\n]*','',probe,flags=re.S)
for name in ('macho_load','macho_run_inits','macho_entry','call_guest','objc_init'):
    assert not re.search(r'\b'+name+r'\s*\(',code), 'Native environment probe must not enter guest/runtime launch: '+name
for name in ('gles_init','gles_eagl_ready','gles_eagl_storage','glDrawArrays','glReadPixels','glFinish'):
    assert re.search(r'\b'+name+r'\s*\(',code), 'Missing real production/GPU check: '+name
assert 'physical_hardware_confirmed\\\":false' in probe
assert 'native-vitagl-environment-v2-no-guest' in probe
assert 'fbo_ready' not in code, 'Probe must not set production readiness flags'
replay=(root/'tools/replay_owner_startup.py').read_text()
assert "phase == 'worker' and address == (symbols['gles_eagl_ready'] & ~1)" in replay
assert "fail('native_graphics_backend_boundary'" in replay
gles=(root/'vendor/ios-loader-boilerplate/source/gles.c').read_text()
assert 'int gles_eagl_ready(void){return fbo_ready && oes;}' in gles
cmake=(root/'vendor/ios-loader-boilerplate/CMakeLists.txt').read_text()
assert re.search(r'option\(IM2_VITAGL_ENV_PROBE[^\n]+ OFF\)',cmake)
assert 'tests/vitagl_environment_probe/main.c' in cmake
helper=(root/'tools/build_vitagl_environment_probe.sh').read_text()
assert '-DIM2_VITAGL_ENV_PROBE=ON' in helper
assert '-DIM2_VITAGL_ENV_PROBE=OFF' in (root/'tools/build_loader_elf.sh').read_text()
assert not re.search(r'\b(vita_create_self|vita_create_vpk|vita-make-fself)\b',cmake+helper)
workflow=(root/'.github/workflows/loader-readiness.yml').read_text()
assert 'default: false' in workflow
assert "if: github.event_name == 'workflow_dispatch' && inputs.device_diagnostics\n        run:" in workflow
assert workflow.count('bash tools/build_device_diagnostics.sh')==1
vm_cmake=(root/'tests/vita_exec_probe/CMakeLists.txt').read_text()
assert re.search(r'option\(IM2_PACKAGE_VM_PROBE[^\n]+ OFF\)',vm_cmake)
assert '-DIM2_PACKAGE_VM_PROBE=OFF' in (root/'tools/build_vm_environment_probe.sh').read_text()
print('PASS: native VitaGL probe scope; guest entry and packaging absent, production readiness and owner barrier unchanged. GPU/device NOT tested.')
