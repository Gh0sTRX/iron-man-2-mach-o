#!/usr/bin/env python3
"""Actual NUL strings and debug-vs-dyld ranges; no game bytes."""
import importlib.util
from pathlib import Path
import struct
spec=importlib.util.spec_from_file_location('scanner',Path(__file__).resolve().parents[1]/'tools/scan_macho.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
b=bytearray(512)
commands=[struct.pack('<II16s8I',1,56,b'__TEXT',0x1000,0x1000,0,512,5,5,0,0),
          struct.pack('<6I',2,24,256,2,288,16),
          struct.pack('<20I',11,80,0,0,0,0,1,1,*([0]*12)),
          struct.pack('<6I',0xc,32,24,0,0,0)+b'/lib\0\0\0\0',
          struct.pack('<5I',0x21,20,0,0,0)]
struct.pack_into('<7I',b,0,0xfeedface,12,6,2,5,sum(map(len,commands)),0)
b[28:28+sum(map(len,commands))]=b''.join(commands)
struct.pack_into('<IBBHI',b,256,1,0xe0,0,0,0)
struct.pack_into('<IBBHI',b,268,8,1,0,0,0)
b[288:304]=b'\0_debug\0_malloc\0'
r=m.scan_bytes(bytes(b))
assert r['segments'][0]['name']=='__TEXT' and r['dylibs']==['/lib']
assert r['undefined_record_count']==2 and r['dynamic_undefined_record_count']==1
assert r['unique_dynamic_undefined_symbols']==['_malloc']
for description,where,value in [('short segment',32,8),('short envelope',20,20),
                               ('bad dynamic range',28+56+24+8+5*4,2)]:
    bad=bytearray(b);struct.pack_into('<I',bad,where,value)
    try:m.scan_bytes(bytes(bad))
    except ValueError:pass
    else:raise AssertionError(description)
bad=bytearray(b);bad[303]=ord('x')
try:m.scan_bytes(bytes(bad))
except ValueError:pass
else:raise AssertionError('unterminated symbol')
for size in (0,4,27,36,100):
    try:m.scan_bytes(bytes(b[:size]))
    except ValueError:pass
    else:raise AssertionError('truncated input')
print('PASS: scanner reads actual NUL names, distinguishes debug/dynamic symbols and rejects malformed ranges')
