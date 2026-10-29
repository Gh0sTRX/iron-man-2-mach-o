#!/usr/bin/env python3
"""Fail-fast structural regression checks for vendored 32-bit Mach-O loader."""
from pathlib import Path
import re
import sys

p = Path(__file__).resolve().parents[1] / "vendor/ios-loader-boilerplate/source/macho.c"
s = p.read_text()
checks = {
    "LC_SEGMENT includes cmd and cmdsize": bool(re.search(r"typedef struct\s*\{\s*uint32_t cmd, cmdsize;\s*char segname\[16\]", s)),
    "Section type uses flags low byte": "(s->flags & 0xffu)" in s,
    "Indirect table uses reserved1": "indirect[s->reserved1 + i]" in s,
    "No invalid section type field": "s->type" not in s,
    "No invalid section r1 field": "s->r1" not in s,
    "No constructor double-slide": "uintptr_t fn = funcs[i] + MACHO_SLIDE" not in s,
    "kubridge executable memory writes": "kuKernelCpuUnrestrictedMemcpy" in s,
    "kubridge cache synchronization": "kuKernelFlushCaches" in s,
    "Single contiguous VM block": 'sceKernelAllocMemBlockForVM("macho_image", image_span)' in s,
    "No split data mapping": 'sceKernelAllocMemBlockForVM("macho_data"' not in s,
    "Runtime slide from actual allocation": 'macho_slide = (uint32_t)(text_real - text_vm)' in s,
    "No fixed slide references": "MACHO_SLIDE" not in s,
    "RBS1 validates full table": 'Validate ALL addresses before touching image memory' in s,
    "RBS1 missing fails closed": 'Required RBS1 table missing' in s,
    "RBS1 import order correct": 0 <= s.find('if (apply_rebase(rebase_path,dysym,d,slice_len) != 0)') < s.find('// Resolve dynamic symbol imports'),
    "Unresolved imports fatal": "Missing %u iOS import bindings; refusing guest execution" in s,
    "Load command envelope bounds checked": 'Invalid Mach-O load command envelope' in s,
    "Vita guest execution is gated": "IM2_ALLOW_UNVERIFIED_VM_EXEC" in s and "guest_exec_ready" in s,
    "Guest entry runtime slide getter": "uint32_t macho_get_runtime_slide(void)" in s,
    "Constructor addresses validated": "im2_run_initializers(" in s,
    "All RBS1 payloads validated": "RBS1 pointer payload invalid" in s,

}
for name, ok in checks.items():
    print(("PASS" if ok else "FAIL") + " " + name)
sys.exit(0 if all(checks.values()) else 1)
