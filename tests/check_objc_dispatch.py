#!/usr/bin/env python3
"""Structural safety checks for Objective-C ARMv6 dispatcher.

This is a source regression check, NOT proof of ObjC ABI compatibility.
"""
from pathlib import Path
import sys

src=(Path(__file__).resolve().parents[1]/
     "vendor/ios-loader-boilerplate/source/objc.c").read_text()
checks={
    "zero-argument selector does not consume va_arg":'if (takes_arg)\n        a = va_arg(ap, uint32_t);' in src,
    "nil selector handled":'if (!sel) {\n        va_end(ap);' in src,
    "C string output checks buffer and capacity":'if (a && max)' in src,
    "selector matcher handles null":'return sel && name && !strcmp(sel, name);' in src,
    "struct-return destination checked":'if (!ret)\n        return;' in src,
    "unknown selectors logged":'log_unknown(self, sel);' in src,
}
for label,ok in checks.items():
    print(("PASS" if ok else "FAIL"),label)
sys.exit(0 if all(checks.values()) else 1)
