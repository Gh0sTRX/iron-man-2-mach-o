#!/usr/bin/env python3
"""Pure synthetic Objective-C Mach-O section census regression."""
import importlib.util
from pathlib import Path
import struct

path=Path(__file__).resolve().parents[1]/"tools"/"audit_objc_sections.py"
spec=importlib.util.spec_from_file_location("objc_audit",path)
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
raw=bytearray(256)
struct.pack_into("<7I",raw,0,0xfeedface,12,6,2,1,124,0)
struct.pack_into("<II",raw,28,1,124)
raw[36:42]=b"__DATA"
struct.pack_into("<IIII",raw,52,0x1000,0x1000,0,256)
struct.pack_into("<I",raw,76,1)
raw[84:100]=b"__objc_classrefs"
raw[100:106]=b"__DATA"
struct.pack_into("<III",raw,116,0x1080,8,200)
struct.pack_into("<II",raw,200,0x1500,0x1600)
result=m.audit(bytes(raw))
assert result["pointer_tables"]["__objc_classrefs"]["slots"]==2
assert result["pointer_tables"]["__objc_classrefs"]["unique_targets"]==2
print("PASS: synthetic Objective-C classref section parsed")
for label,offset,value in [("short command",32,500),("short section",32,56)]:
    bad=bytearray(raw)
    struct.pack_into("<I",bad,offset,value)
    try:m.audit(bytes(bad))
    except ValueError:print("PASS rejected",label)
    else:raise AssertionError(label)
