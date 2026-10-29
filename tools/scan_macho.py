#!/usr/bin/env python3
"""Read-only scanner for a locally owned, thin 32-bit little-endian Mach-O executable."""
import argparse, json, pathlib, struct

def scan(path):
    b = pathlib.Path(path).read_bytes()
    if len(b) < 28 or b[:4] != bytes.fromhex('cefaedfe'):
        raise ValueError('Expected thin little-endian 32-bit Mach-O')
    _, cpu, subtype, kind, ncmds, cmds_size, flags = struct.unpack_from('<IiiIIII', b)
    pos=28; seg=[]; libs=[]; encryption=None; symbols=None
    if pos+cmds_size>len(b): raise ValueError('Load commands out of bounds')
    for _ in range(ncmds):
        if pos+8>len(b): raise ValueError('Truncated load command')
        cmd,size=struct.unpack_from('<II',b,pos)
        if size<8 or pos+size>len(b): raise ValueError('Invalid load command size')
        if cmd==1:
            name=b[pos+8:pos+24].split(b'\\0')[0].decode('ascii','replace')
            vm,vs=struct.unpack_from('<II',b,pos+24)
            seg.append(dict(name=name,vmaddr=hex(vm),vmsize=hex(vs)))
        elif cmd==0x21: encryption=struct.unpack_from('<III',b,pos+8)[2]
        elif cmd in (0xc,0xd,0x18,0x1f,0x80000018):
            off=struct.unpack_from('<I',b,pos+8)[0]
            if off>=size: raise ValueError('Invalid dylib offset')
            libs.append(b[pos+off:pos+size].split(b'\\0')[0].decode('utf8','replace'))
        elif cmd==2: symbols=struct.unpack_from('<IIII',b,pos+8)
        pos+=size
    names=[];count=0
    if symbols:
        symoff,count,stroff,strsize=symbols
        if symoff+12*count>len(b) or stroff+strsize>len(b):raise ValueError('Symbol table out of bounds')
        for i in range(count):
            ix,typ,section,desc,val=struct.unpack_from('<IBBHI',b,symoff+i*12)
            if typ & 0xe == 0 and ix<strsize:
                end=b.find(b'\\0',stroff+ix,stroff+strsize)
                if end!=-1:names.append(b[stroff+ix:end].decode('latin1'))
    return dict(cpu=cpu,subtype=subtype,filetype=kind,cryptid=encryption,segments=seg,dylibs=libs,symbol_count=count,undefined_record_count=len(names),unique_undefined_symbols=sorted(set(names)))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('macho');a=p.parse_args()
    print(json.dumps(scan(a.macho),indent=2))
