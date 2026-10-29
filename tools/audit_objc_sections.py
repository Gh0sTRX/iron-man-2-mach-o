#!/usr/bin/env python3
"""Read-only 32-bit iOS Mach-O Objective-C section census.

Use with an owner-supplied decrypted Mach-O executable, not committed game files.
This intentionally does not invent Objective-C classes or execute guest code.
"""
import argparse
import json
from pathlib import Path
import struct
import zipfile

def audit(data):
    if len(data)<28 or data[:4]!=b"\xce\xfa\xed\xfe":
        raise ValueError("Expected thin little-endian 32-bit Mach-O")
    ncmd,cmdbytes=struct.unpack_from("<II",data,16)
    if ncmd>cmdbytes//8 or 28+cmdbytes>len(data):
        raise ValueError("Invalid load-command envelope")
    offset=28
    sections={}
    for _ in range(ncmd):
        if offset+8>28+cmdbytes:raise ValueError("Short load command")
        cmd,size=struct.unpack_from("<II",data,offset)
        if size<8 or offset+size>28+cmdbytes:raise ValueError("Invalid command length")
        if cmd==1:
            if size<56:raise ValueError("Short segment")
            nsects=struct.unpack_from("<I",data,offset+48)[0]
            if nsects>(size-56)//68:raise ValueError("Short section table")
            for j in range(nsects):
                so=offset+56+68*j
                name=data[so:so+16].split(b"\0",1)[0].decode("ascii")
                vm,length,fileoff=struct.unpack_from("<III",data,so+32)
                if fileoff and fileoff+length>len(data):
                    raise ValueError("Section outside Mach-O file")
                if name.startswith("__objc_") or name=="__cfstring":
                    sections[name]={"vmaddr":vm,"bytes":length,"fileoff":fileoff}
        offset+=size
    pointers={}
    for name in ("__objc_selrefs","__objc_classrefs","__objc_superrefs",
                 "__objc_classlist","__objc_protolist"):
        if name not in sections:continue
        sec=sections[name]
        if sec["bytes"]%4:raise ValueError(name+" not word-aligned")
        vals=struct.unpack_from("<%dI"%(sec["bytes"]//4),data,sec["fileoff"])
        pointers[name]={
            "slots":len(vals),"nonzero":sum(v!=0 for v in vals),
            "unique_targets":len(set(vals)),
            "sample_original_addresses":[hex(v) for v in vals[:8]]
        }
    return {"sections":sections,"pointer_tables":pointers,
            "warning":"Section counts do not establish working Objective-C runtime/ABI."}

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--ipa",type=Path)
    p.add_argument("--macho",type=Path)
    p.add_argument("--output",type=Path)
    args=p.parse_args()
    if bool(args.ipa)==bool(args.macho):
        p.error("Supply exactly one of --ipa or --macho")
    if args.ipa:
        with zipfile.ZipFile(args.ipa) as z:
            entries=[n for n in z.namelist() if n.startswith("Payload/") and n.endswith(".app/IronMan2")]
            if len(entries)!=1:raise ValueError("IronMan2 executable missing or ambiguous")
            data=z.read(entries[0])
    else:data=args.macho.read_bytes()
    result=json.dumps(audit(data),indent=2)
    print(result)
    if args.output:args.output.write_text(result+"\n")

if __name__=="__main__":main()
