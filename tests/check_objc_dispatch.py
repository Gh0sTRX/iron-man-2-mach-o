#!/usr/bin/env python3
"""Structural guards complement behavioral Foundation/core and machine-code ABI tests."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
s=(root/'vendor/ios-loader-boilerplate/source/objc.c').read_text()
checks={
    'assembly dispatch retains explicit ABI kind':'F_ABI(objc_msgSend, im2_objc_msgSend_entry)' in s,
    'unknown receiver/selector/ABI is rejected':'fatal_error("Unsupported ObjC receiver/selector/return ABI:' in s,
    'guest aggregate return ABI remains gated':'target.stret_size==SIZE_MAX' in s,
    'Foundation uses actual class module':'im2_foundation_init(root,&foundation)' in s,
    'no universal dummy object':'dummy_obj' not in s and 'ret_obj' not in s,
    'no foreign game version fabricated':'1.1.41' not in s,
    'class import entries are data':'class_imports[i]=(import_entry)' in s and 'IM2_IMPORT_DATA' in s,
}
for name,ok in checks.items():print(('PASS' if ok else 'FAIL'),name)
sys.exit(0 if all(checks.values()) else 1)
