#!/usr/bin/env python3
"""Read-only owner IPA UIScreen selector/Info.plist audit; never uploads IPA."""
import argparse
import hashlib
import json
import plistlib
import struct
import zipfile
from pathlib import Path

def inspect(ipa: Path):
    with zipfile.ZipFile(ipa) as z:
        prefix="Payload/IronMan2.app/"
        plist=plistlib.loads(z.read(prefix+"Info.plist"))
        image=z.read(prefix+"IronMan2")
    u32=lambda p:struct.unpack_from("<I",image,p)[0]
    if u32(0)!=0xfeedface or u32(4)!=12:
        raise ValueError("expected thin little-endian ARM32 Mach-O")
    count,cmdbytes=u32(16),u32(20)
    if count>128 or 28+cmdbytes>len(image):
        raise ValueError("bad load-command header")
    pos=28
    sections=[]
    for _ in range(count):
        cmd,size=u32(pos),u32(pos+4)
        if size<8 or pos+size>28+cmdbytes:
            raise ValueError("invalid command envelope")
        if cmd==1:
            n=u32(pos+48)
            if size<56 or n>(size-56)//68:
                raise ValueError("invalid segment sections")
            for j in range(n):
                p=pos+56+j*68
                name=image[p:p+16].split(b"\x00")[0].decode("ascii")
                sections.append((name,u32(p+32),u32(p+36),u32(p+40)))
        pos+=size
    def mapped(va,length):
        for _,start,size,offset in sections:
            if start<=va and va+length<=start+size and offset+va-start+length<=len(image):
                p=offset+va-start
                return image[p:p+length]
        raise ValueError("unmapped selector pointer "+hex(va))
    ref=next(x for x in sections if x[0]=="__objc_selrefs")
    if ref[2]%4:raise ValueError("unaligned __objc_selrefs")
    expected=["mainScreen","bounds","makeKeyAndVisible","initWithFrame:"]
    result={name:[] for name in expected}
    for offset in range(0,ref[2],4):
        slot=ref[1]+offset
        target=struct.unpack("<I",mapped(slot,4))[0]
        for name in expected:
            if mapped(target,len(name)+1)==name.encode()+b"\x00":
                result[name].append(hex(slot))
    return {
        "original_executable_sha256":hashlib.sha256(image).hexdigest(),
        "minimum_os":plist.get("MinimumOSVersion"),
        "status_bar_hidden":plist.get("UIStatusBarHidden"),
        "selector_references":result,
        "geometry_is_hardware_assumption_not_macho_fact":True,
    }

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--ipa",type=Path,required=True)
    args=parser.parse_args()
    found=inspect(args.ipa)
    if found["status_bar_hidden"] is not True:
        raise ValueError("original status-bar setting changed")
    for name in ("mainScreen","bounds"):
        if len(found["selector_references"][name])!=1:
            raise ValueError("owner UIScreen selector reference changed: "+name)
    print(json.dumps(found,indent=2))

if __name__=="__main__":
    main()
