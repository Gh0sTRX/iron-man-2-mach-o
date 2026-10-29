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
}
for name, ok in checks.items():
    print(("PASS" if ok else "FAIL") + " " + name)
sys.exit(0 if all(checks.values()) else 1)
