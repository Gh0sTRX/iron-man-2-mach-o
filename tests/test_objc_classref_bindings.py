#!/usr/bin/env python3
"""Synthetic fixture for ARMv6 Mach-O Objective-C class external relocations."""
import importlib.util
from pathlib import Path
import struct

p=Path(__file__).resolve().parents[1]/"tools"/"audit_objc_classref_bindings.py"
spec=importlib.util.spec_from_file_location("classrefs",p)
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
d=bytearray(512)
# Mach header: 32-bit ARM, 3 commands (segment, symtab, dysymtab).
struct.pack_into("<7I",d,0,0xfeedface,12,6,2,3,124+24+80,0)
off=28
struct.pack_into("<II",d,off,1,124)
d[off+8:off+14]=b"__DATA"
struct.pack_into("<8I",d,off+24,0x1000,0x1000,0,512,7,3,1,0)
sec=off+56
d[sec:sec+16]=b"__objc_classrefs"
d[sec+16:sec+22]=b"__DATA"
struct.pack_into("<9I",d,sec+32,0x1100,8,256,2,0,0,0,0,0)
off+=124
struct.pack_into("<6I",d,off,2,24,280,1,300,32)
off+=24
struct.pack_into("<II",d,off,11,80)
# external relocation table at 340; indirect table empty
struct.pack_into("<II",d,off+8+14*4,340,1)
# nlist, symbol table string
struct.pack_into("<IBBHI",d,280,1,1,0,0,0)
d[300:302]=b"\0_"
name=b"_OBJC_CLASS_$_NSString\0"
d[301:301+len(name)]=name
# relocation target at VA 0x1100, r_extern=1, length=2, type=0
struct.pack_into("<II",d,340,0x1100,0x0c000000)
result=m.inspect(bytes(d))
assert result["total_slots"]==2,result
assert result["kinds"]=={"external_relocation":1,"unresolved_unclassified":1},result
assert result["unique_external_symbols"]==["_OBJC_CLASS_$_NSString"],result
print("PASS synthetic ObjC external relocation to class name")
for desc,loc,value in [("bad extrel size",off+8+15*4,500),
                        ("invalid extrel symbol",340+4,0x0c000003)]:
    bad=bytearray(d)
    struct.pack_into("<I",bad,loc,value)
    try:m.inspect(bytes(bad))
    except ValueError:print("PASS reject",desc)
    else:raise AssertionError(desc)
