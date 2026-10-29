#!/usr/bin/env python3
"""Original-machine-independent fixture for guest UIKit parent relocation audit."""
import importlib.util
from pathlib import Path
import struct

script=Path(__file__).resolve().parents[1]/"tools"/"audit_owner_guest_superclasses.py"
spec=importlib.util.spec_from_file_location("guestparents",script)
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
data=bytearray(1024)
def pack(off,*words):struct.pack_into("<"+"I"*len(words),data,off,*words)
# ARM32 Mach-O with LC_SEGMENT(2 sections), LC_SYMTAB, LC_DYSYMTAB.
pack(0,0xfeedface,12,6,2,3,296,0)
pack(28,1,192)
data[36:42]=b"__DATA"
pack(28+24,0x1000,0x300,340,320,7,3,2,0)
p=28+56
data[p:p+16]=b"__objc_classlist"
data[p+16:p+22]=b"__DATA"
pack(p+32,0x1000,4,340,2,0,0,0,0,0)
p+=68
data[p:p+16]=b"__data"
data[p+16:p+22]=b"__DATA"
pack(p+32,0x1100,0x140,344,2,0,0,0,0,0)
pack(220,2,24,700,2,730,100)
pack(244,11,80)
pack(244+8+14*4,900,2)
pack(340,0x1100)
pack(344,0x1114,0,0,0,0x1128)
pack(364,0,0,0,0,0)
pack(384,0,36,128,0,0x1168,0,0,0,0,0)
data[344+0x68:344+0x68+9]=b"EAGLView\0"
one=b"_OBJC_CLASS_$_UIView\0"
two=b"_OBJC_METACLASS_$_UIView\0"
data[730]=0
data[731:731+len(one)]=one
data[731+len(one):731+len(one)+len(two)]=two
pack(700,1)
pack(712,1+len(one))
pack(900,0x1104,0x0c000000)
pack(908,0x1118,0x0c000001)
expect=[{"class":"EAGLView","superclass":"UIView",
         "instance_start":36,"instance_size":128}]
assert m.inspect(bytes(data))==expect
for label,off,value in [
    ("bad metaclass symbol index",908+4,0x0c000005),
    ("missing superclass relocation",900,0x1500)
]:
    invalid=bytearray(data)
    struct.pack_into("<I",invalid,off,value)
    try:m.inspect(bytes(invalid))
    except ValueError:print("PASS reject",label)
    else:raise AssertionError(label)
print("PASS: ARM32 guest superclass parent/name and instance_start audit")
