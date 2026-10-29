#!/usr/bin/env python3
"""Read-only thin ARM32 Mach-O scanner. Generic nlist counts are not dyld imports."""
import argparse
import json
from pathlib import Path
import struct


def scan_bytes(data):
    def unpack(fmt, off):
        n=struct.calcsize('<'+fmt)
        if off<0 or off+n>len(data):raise ValueError('Mach-O read out of bounds')
        return struct.unpack_from('<'+fmt,data,off)
    def text(off,end):
        finish=data.find(b'\0',off,end)
        if finish<0:raise ValueError('Unterminated Mach-O string')
        return data[off:finish].decode('latin1')
    magic,cpu,subtype,kind,ncmds,cmdsize,flags=unpack('IiiIIII',0)
    if (magic,cpu,kind)!=(0xfeedface,12,2):
        raise ValueError('Expected thin little-endian ARM32 MH_EXECUTE')
    end=28+cmdsize
    if end>len(data) or ncmds>cmdsize//8:raise ValueError('Invalid command envelope')
    pos=28;segments=[];libs=[];encryption=None;symtab=None;dynamic=None
    for _ in range(ncmds):
        if pos+8>end:raise ValueError('Truncated command header')
        cmd,size=unpack('II',pos)
        if size<8 or size%4 or pos+size>end:raise ValueError('Invalid command length')
        if cmd==1:
            if size<56:raise ValueError('Short segment command')
            vm,vs,fileoff,filesize,maxprot,prot,nsects,sflags=unpack('8I',pos+24)
            if nsects>(size-56)//68 or filesize>vs or fileoff+filesize>len(data) or vm+vs>0xffffffff:
                raise ValueError('Invalid segment range')
            name=data[pos+8:pos+24].split(b'\0',1)[0].decode('ascii','replace')
            segments.append(dict(name=name,vmaddr=hex(vm),vmsize=hex(vs)))
        elif cmd==0x21:
            if size<20:raise ValueError('Short encryption command')
            off,length,encryption=unpack('3I',pos+8)
            if off+length>len(data):raise ValueError('Encryption range outside file')
        elif cmd in (0xc,0x18,0x1f,0x80000018,0x8000001f,0x80000023):
            if size<24:raise ValueError('Short dylib command')
            off=unpack('I',pos+8)[0]
            if not 24<=off<size:raise ValueError('Invalid dylib name offset')
            libs.append(text(pos+off,pos+size))
        elif cmd==2:
            if size!=24 or symtab is not None:raise ValueError('Invalid/duplicate symbol command')
            symtab=unpack('4I',pos+8)
        elif cmd==11:
            if size!=80 or dynamic is not None:raise ValueError('Invalid/duplicate dysymtab')
            dynamic=unpack('18I',pos+8)
        pos+=size
    if pos!=end:raise ValueError('Command count/size mismatch')
    symbols=[];names=[];count=0
    if symtab:
        symoff,count,stroff,strsize=symtab
        if symoff+12*count>len(data) or stroff+strsize>len(data):raise ValueError('Symbol table outside file')
        for i in range(count):
            index,typ,section,desc,value=unpack('IBBHI',symoff+i*12)
            if index>=strsize:raise ValueError('Symbol string index out of range')
            name=text(stroff+index,stroff+strsize)
            symbols.append((name,typ))
            if typ & 0xe==0:names.append(name)
    dynamic_names=None
    if dynamic:
        first,n=dynamic[4:6]
        if not symtab or first+n>count:raise ValueError('Dynamic undefined range outside symbols')
        selected=symbols[first:first+n]
        if any(typ & (0xe0|0xe) for _,typ in selected):raise ValueError('Dysymtab undefined range contains defined/debug symbols')
        dynamic_names=[name for name,_ in selected]
    return dict(cpu=cpu,subtype=subtype,filetype=kind,cryptid=encryption,segments=segments,dylibs=libs,
                symbol_count=count,undefined_record_count=len(names),unique_undefined_symbols=sorted(set(names)),
                dynamic_undefined_record_count=None if dynamic_names is None else len(dynamic_names),
                unique_dynamic_undefined_symbols=None if dynamic_names is None else sorted(set(dynamic_names)),
                note='undefined_record_count is generic nlist metadata; use LC_DYSYMTAB for dynamic import scope')


def scan(path):
    with Path(path).open('rb') as source:data=source.read(128*1024*1024+1)
    if len(data)>128*1024*1024:raise ValueError('Executable exceeds 128 MiB research limit')
    return scan_bytes(data)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('macho');a=p.parse_args()
    try:print(json.dumps(scan(a.macho),indent=2))
    except (ValueError,OSError) as error:p.exit(1,f'rejected: {error}\n')
