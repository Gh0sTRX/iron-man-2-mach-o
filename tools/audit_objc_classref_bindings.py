#!/usr/bin/env python3
"""Resolve original ARMv6 Mach-O Objective-C classref slots to indirect symbols.

Read-only. No guest execution, no fake ObjC class objects, no IPA in Git.
"""
import argparse
import json
from pathlib import Path
import struct
import zipfile

def u32(data, off):
    if off < 0 or off + 4 > len(data):
        raise ValueError("Mach-O read out of bounds")
    return struct.unpack_from("<I", data, off)[0]

def inspect(data):
    if len(data) < 28 or data[:4] != b"\xce\xfa\xed\xfe":
        raise ValueError("Expected thin ARM 32-bit little-endian Mach-O")
    if u32(data,4) != 12:
        raise ValueError("Not an ARM Mach-O")
    ncmd, nbytes = u32(data,16), u32(data,20)
    if ncmd > nbytes//8 or 28+nbytes > len(data):
        raise ValueError("Invalid command envelope")
    off=28
    sections=[]
    symtab=None
    dysym=None
    for _ in range(ncmd):
        cmd, size=u32(data,off),u32(data,off+4)
        if size<8 or off+size>28+nbytes:
            raise ValueError("Invalid command length")
        if cmd==1:
            if size<56:raise ValueError("Short segment")
            count=u32(data,off+48)
            if count>(size-56)//68:raise ValueError("Truncated section list")
            for j in range(count):
                p=off+56+j*68
                name=data[p:p+16].split(b"\0",1)[0].decode("ascii")
                addr,length,fileoff=u32(data,p+32),u32(data,p+36),u32(data,p+40)
                flags,reserved1=u32(data,p+56),u32(data,p+60)
                sections.append((name,addr,length,fileoff,flags,reserved1))
        elif cmd==2:
            if size<24:raise ValueError("Short symtab command")
            symtab=tuple(u32(data,off+8+4*j) for j in range(4))
        elif cmd==11:
            if size<80:raise ValueError("Short dysymtab command")
            dysym=tuple(u32(data,off+8+4*j) for j in range(18))
        off+=size
    if symtab is None or dysym is None:
        raise ValueError("Missing symbol tables")
    symoff,nsyms,stroff,strsize=symtab
    indirectoff,nindirect=dysym[12:14]
    if (symoff+nsyms*12>len(data) or stroff+strsize>len(data)
            or indirectoff+nindirect*4>len(data)):
        raise ValueError("Symbol table bounds")
    # Objective-C classrefs in this original iOS build are S_REGULAR,
    # NOT S_NON_LAZY_SYMBOL_POINTERS. Their unresolved slots are instead
    # recorded in LC_DYSYMTAB external relocation entries (extreloff).
    extrel_off,extrel_count=dysym[14:16]
    if extrel_off+extrel_count*8>len(data):
        raise ValueError("External relocation table out of bounds")
    extrel={}
    for i in range(extrel_count):
        p=extrel_off+i*8
        reloc_addr=u32(data,p)
        info=u32(data,p+4)
        if reloc_addr&0x80000000:
            raise ValueError("Scattered external relocation unsupported")
        if info&0x08000000: # r_extern
            symbol_idx=info&0x00ffffff
            if symbol_idx>=nsyms:raise ValueError("External relocation symbol out of range")
            start=u32(data,symoff+symbol_idx*12)
            if start>=strsize:raise ValueError("Relocation string index out of range")
            end=data.find(b"\0",stroff+start,stroff+strsize)
            if end<0:raise ValueError("Unterminated relocation symbol")
            extrel[reloc_addr]=data[stroff+start:end].decode("utf8","replace")
    results=[]
    for name,addr,length,fileoff,flags,reserved1 in sections:
        if name not in ("__objc_classrefs","__objc_superrefs"):
            continue
        if length%4 or fileoff+length>len(data):
            raise ValueError("Bad ObjC pointer section")
        for i in range(length//4):
            va=addr+4*i
            raw=u32(data,fileoff+4*i)
            entry={"section":name,"slot_vmaddr":hex(va),"original_value":hex(raw)}
            if (flags&0xff) in (6,7):
                if reserved1+i>=nindirect:raise ValueError("Indirect table index out of range")
                idx=u32(data,indirectoff+4*(reserved1+i))
                if idx&0x80000000:
                    entry["kind"]="local"
                elif idx&0x40000000:
                    entry["kind"]="absolute"
                else:
                    if idx>=nsyms:raise ValueError("Symbol index out of range")
                    p=symoff+idx*12
                    start=u32(data,p)
                    if start>=strsize:raise ValueError("String index out of range")
                    end=data.find(b"\0",stroff+start,stroff+strsize)
                    if end<0:raise ValueError("Unterminated symbol name")
                    entry["kind"]="external"
                    entry["symbol"]=data[stroff+start:end].decode("utf8","replace")
            else:
                if va in extrel:
                    entry["kind"]="external_relocation"
                    entry["symbol"]=extrel[va]
                elif raw:
                    entry["kind"]="local_initialized"
                else:
                    entry["kind"]="unresolved_unclassified"
            results.append(entry)
    kinds={}
    for row in results:
        kinds[row["kind"]]=kinds.get(row["kind"],0)+1
    return {"total_slots":len(results),"kinds":kinds,
            "zero_original_values":sum(x["original_value"]=="0x0" for x in results),
            "unique_external_symbols":sorted({x["symbol"] for x in results if "symbol" in x}),
            "slots":results}

def main():
    p=argparse.ArgumentParser()
    src=p.add_mutually_exclusive_group(required=True)
    src.add_argument("--ipa",type=Path)
    src.add_argument("--macho",type=Path)
    p.add_argument("--output",type=Path)
    a=p.parse_args()
    if a.ipa:
        with zipfile.ZipFile(a.ipa) as z:
            matches=[n for n in z.namelist() if n.startswith("Payload/") and n.endswith(".app/IronMan2")]
            if len(matches)!=1:raise ValueError("Original IronMan2 Mach-O not found")
            data=z.read(matches[0])
    else:data=a.macho.read_bytes()
    output=json.dumps(inspect(data),indent=2,ensure_ascii=False)
    print(output)
    if a.output:a.output.write_text(output+"\n")
if __name__=="__main__":main()
