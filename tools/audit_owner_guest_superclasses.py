#!/usr/bin/env python3
"""Audit guest class parents from owner-provided ARM32 Mach-O without executing it."""
import argparse
import json
from pathlib import Path
import struct
import zipfile

def inspect(data):
    if len(data)<28 or data[:4]!=b"\xce\xfa\xed\xfe":
        raise ValueError("Expected thin little-endian ARM Mach-O")
    def word(off):
        if off<0 or off+4>len(data):raise ValueError("Invalid offset")
        return struct.unpack_from("<I",data,off)[0]
    if word(4)!=12:raise ValueError("Mach-O CPU must be ARM")
    count,total=word(16),word(20)
    if count>total//8 or 28+total>len(data):
        raise ValueError("Invalid load command envelope")
    sections=[];symtab=None;dysym=None;pos=28
    for _ in range(count):
        cmd,size=word(pos),word(pos+4)
        if size<8 or pos+size>28+total:
            raise ValueError("Invalid command")
        if cmd==1:
            if size<56:raise ValueError("Short segment")
            n=word(pos+48)
            if n>(size-56)//68:raise ValueError("Truncated section list")
            for j in range(n):
                p=pos+56+j*68
                name=data[p:p+16].split(b"\0")[0].decode("ascii")
                sections.append((name,word(p+32),word(p+36),word(p+40)))
        elif cmd==2:
            if size<24:raise ValueError("Short symtab")
            symtab=tuple(word(pos+8+4*j) for j in range(4))
        elif cmd==11:
            if size<80:raise ValueError("Short dysymtab")
            dysym=tuple(word(pos+8+4*j) for j in range(18))
        pos+=size
    if not symtab or not dysym:raise ValueError("Missing symbol tables")
    def filepos(addr,size):
        for _,start,length,offset in sections:
            if start<=addr and addr+size<=start+length:
                out=offset+addr-start
                if out+size<=len(data):return out
        raise ValueError("VM pointer outside mapped sections: "+hex(addr))
    def ptr(addr):return word(filepos(addr,4))
    def cstring(addr):
        out=bytearray()
        for i in range(256):
            ch=data[filepos(addr+i,1)]
            if not ch:return out.decode("utf8")
            out.append(ch)
        raise ValueError("Unterminated ObjC class name")
    symoff,nsyms,stroff,strsize=symtab
    if symoff+nsyms*12>len(data) or stroff+strsize>len(data):
        raise ValueError("Invalid symbol bounds")
    def symbol(index):
        if index>=nsyms:raise ValueError("Relocation symbol index out of range")
        start=word(symoff+index*12)
        if start>=strsize:raise ValueError("Symbol string offset invalid")
        end=data.find(b"\0",stroff+start,stroff+strsize)
        if end<0:raise ValueError("Unterminated symbol")
        return data[stroff+start:end].decode("utf8")
    ext_off,ext_count=dysym[14:16]
    if ext_off+ext_count*8>len(data):
        raise ValueError("External relocation table out of bounds")
    reloc={}
    for i in range(ext_count):
        off=ext_off+i*8
        addr,info=word(off),word(off+4)
        if addr&0x80000000:continue
        if info&0x08000000:reloc[addr]=symbol(info&0xffffff)
    lists=[s for s in sections if s[0]=="__objc_classlist"]
    if len(lists)!=1 or lists[0][2]%4:
        raise ValueError("Missing/malformed guest class list")
    _,addr,size,_=lists[0];records=[]
    for i in range(size//4):
        cls=ptr(addr+i*4)
        isa=ptr(cls);data_addr=ptr(cls+16)
        if not cls or not isa or not data_addr:
            raise ValueError("Malformed Objective-C class")
        name=cstring(ptr(data_addr+16))
        parent=reloc.get(cls+4);meta_parent=reloc.get(isa+4)
        if not parent or not meta_parent:
            raise ValueError("Missing class/metaclass parent relocation: "+name)
        cp="_OBJC_CLASS_$_";mp="_OBJC_METACLASS_$_"
        if not parent.startswith(cp) or not meta_parent.startswith(mp) or parent[len(cp):]!=meta_parent[len(mp):]:
            raise ValueError("Inconsistent class/metaclass parent: "+name)
        records.append({"class":name,"superclass":parent[len(cp):],
                        "instance_start":ptr(data_addr+4),
                        "instance_size":ptr(data_addr+8)})
    return records

def main():
    parser=argparse.ArgumentParser()
    src=parser.add_mutually_exclusive_group(required=True)
    src.add_argument("--ipa",type=Path)
    src.add_argument("--macho",type=Path)
    parser.add_argument("--expected-json",type=Path)
    args=parser.parse_args()
    if args.ipa:
        with zipfile.ZipFile(args.ipa) as z:
            entries=[p for p in z.namelist() if p.startswith("Payload/") and p.endswith(".app/IronMan2")]
            if len(entries)!=1:raise ValueError("Original IronMan2 executable not found uniquely")
            data=z.read(entries[0])
    else:data=args.macho.read_bytes()
    records=inspect(data)
    if args.expected_json:
        expected=json.loads(args.expected_json.read_text())["required_guest_subclasses"]
        for entry in expected:
            found=next((r for r in records if r["class"]==entry["guest_class"]),None)
            if found is None or (found["superclass"],found["instance_start"],found["instance_size"])!=(
                entry["imported_superclass"],entry["instance_start"],entry["instance_size"]):
                raise ValueError("Owner superclass manifest drift: "+entry["guest_class"])
    print(json.dumps({"class_count":len(records),"classes":records},indent=2))

if __name__=="__main__":main()
