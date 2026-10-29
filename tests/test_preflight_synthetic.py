#!/usr/bin/env python3
"""Own synthetic ARM image exercises the same preflight used for the private original."""
import importlib.util
from pathlib import Path
import struct
spec=importlib.util.spec_from_file_location('preflight',Path(__file__).resolve().parents[1]/'tools/preflight_original_image.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
b=bytearray(0x4000)
def seg(name,vm,fo):return struct.pack('<II16s8I',1,56,name.encode(),vm,0x1000,fo,0x1000,7,3,0,0)
commands=[seg('__TEXT',0x1000,0),seg('__DATA',0x2000,0x1000),struct.pack('<6I',2,24,0x2100,1,0x2110,4),struct.pack('<20I',11,80,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0x2120,1,0,0),struct.pack('<5I',0x21,20,0,0,0)]
struct.pack_into('<7I',b,0,0xfeedface,12,6,2,5,sum(map(len,commands)),0)
b[28:28+sum(map(len,commands))]=b''.join(commands)
struct.pack_into('<I',b,0x1000,0x1101)
struct.pack_into('<IBBHI',b,0x2100,1,1,0,0,0);b[0x2110:0x2114]=b'\0_m\0'
struct.pack_into('<2I',b,0x2120,0x2004,0x0c000000)
rbs=struct.pack('<4sII',b'RBS1',1,0x2000)
r=m.preflight(bytes(b),rbs);assert r['external_relocations']==1 and r['rbs1_external_relocation_overlap']==0
for value in (0x0d000000,0x1c000000,0x0a000000,0x04000000,0x0c000001):
    bad=bytearray(b);struct.pack_into('<I',bad,0x2124,value)
    try:m.preflight(bytes(bad),rbs)
    except ValueError:pass
    else:raise AssertionError('bad relocation encoding/index accepted')
for address in (0x80002004,0x2005,0x4000):
    bad=bytearray(b);struct.pack_into('<I',bad,0x2120,address)
    try:m.preflight(bytes(bad),rbs)
    except ValueError:pass
    else:raise AssertionError('bad relocation target accepted')
dysym=28+56+56+24
bad=bytearray(b);struct.pack_into('<I',bad,dysym+8+15*4,2);struct.pack_into('<2I',bad,0x2128,0x2004,0x0c000000)
try:m.preflight(bytes(bad),rbs)
except ValueError as e:assert 'Duplicate' in str(e)
else:raise AssertionError('duplicate target accepted')
bad=bytearray(b);struct.pack_into('<I',bad,0x1004,0x1100)
overlap=struct.pack('<4sIII',b'RBS1',2,0x2000,0x2004)
try:m.preflight(bytes(bad),overlap)
except ValueError as e:assert 'overlaps external' in str(e)
else:raise AssertionError('rebase/extern overlap accepted')
bad=bytearray(b);struct.pack_into('<I',bad,28+56+56+24+80+16,1)
try:m.preflight(bytes(bad),rbs)
except ValueError as e:assert 'Encrypted' in str(e)
else:raise AssertionError('encrypted input accepted')
print('PASS: original-image preflight rejects unsupported/duplicate/out-of-range external relocations, overlaps and encryption')
