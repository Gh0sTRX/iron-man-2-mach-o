#!/usr/bin/env python3
"""Inspect ABI flags and target addresses in the actual linked ARM ELF import table."""
import argparse
from pathlib import Path
import struct

p=argparse.ArgumentParser();p.add_argument('--elf',required=True);a=p.parse_args()
b=Path(a.elf).read_bytes()
assert b[:7]==b'\x7fELF\x01\x01\x01'
h=struct.unpack_from('<HHIIIIIHHHHHH',b,16)
assert h[1]==40  # EM_ARM
shoff,shsize,shcount=h[5],h[10],h[11]
sections=[struct.unpack_from('<10I',b,shoff+shsize*i) for i in range(shcount)]
def read(va,n):
    for s in sections:
        if s[1]!=8 and s[3]<=va and va+n<=s[3]+s[5]:
            off=s[4]+va-s[3]
            return b[off:off+n]
    raise AssertionError(f'unmapped ELF address {va:x}')
def cstring(va):
    text=bytearray()
    for i in range(256):
        c=read(va+i,1)
        if c==b'\0':return text.decode()
        text.extend(c)
    raise AssertionError('unterminated ELF string')
symbols={}
for s in sections:
    if s[1]!=2:continue
    strings=sections[s[6]]
    for off in range(s[4],s[4]+s[5],s[9]):
        name,value,size,info,other,index=struct.unpack_from('<IIIBBH',b,off)
        end=b.index(0,strings[4]+name,strings[4]+strings[5])
        symbols[b[strings[4]+name:end].decode()]=value
count=struct.unpack('<I',read(symbols['objc_imports_count'],4))[0]
entries={}
for i in range(count):
    name,target,kind=struct.unpack('<3I',read(symbols['objc_imports']+12*i,12))
    entries[cstring(name)]=(target,kind)
for name in ('objc_msgSend','objc_msgSend_stret','objc_msgSendSuper2','objc_msgSendSuper2_stret'):
    target,kind=entries['_'+name]
    assert kind==2, (name,'assembly dispatcher must bypass generic stack-copy thunk')
    assert target==symbols['im2_'+name+'_entry']
for name in ('objc_getClass','CFStringGetCString','CFRelease','CFRetain','NSClassFromString','objc_enumerationMutation'):
    target,kind=entries['_'+name]
    assert target and kind==0, (name,'C bridges must use the ABI thunk')
assert '_UIApplicationMain' in entries
entry,kind=entries['_UIApplicationMain']
assert entry and kind==0, 'Legacy UIApplicationMain must bind to the nonempty native C launch stage'
eagl_count=struct.unpack('<I',read(symbols['eagl_imports_count'],4))[0]
assert eagl_count==3
eagl={}
for i in range(eagl_count):
    name,target,kind=struct.unpack('<3I',read(symbols['eagl_imports']+12*i,12))
    eagl[cstring(name)]=(target,kind)
assert set(eagl)=={
    '_kEAGLDrawablePropertyRetainedBacking',
    '_kEAGLDrawablePropertyColorFormat',
    '_kEAGLColorFormatRGB565'
}
assert all(target and kind==1 for target,kind in eagl.values()), 'EAGL NSString globals must be bound as data, never function thunks'
print('PASS: linked ARM ELF includes ObjC message dispatch, UIApplicationMain and three EAGL NSString data constants')
