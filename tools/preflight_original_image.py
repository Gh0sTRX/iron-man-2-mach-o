#!/usr/bin/env python3
"""Host-only Iron Man 2 original iOS Mach-O + RBS1 preflight. NEVER executes IPA code."""
import argparse
import json
from pathlib import Path
import struct
import zipfile

def preflight(data, rbs):
    if len(data)<28 or data[:4]!=bytes.fromhex("cefaedfe"):
        raise ValueError("Expected thin little-endian 32-bit Mach-O")
    magic,cpu,subtype,kind,ncmd,cmdbytes,flags=struct.unpack_from("<7I",data)
    if cpu!=12 or ncmd>cmdbytes//8 or 28+cmdbytes>len(data):
        raise ValueError("Invalid CPU/load command envelope")
    off=28
    segs={}
    sym=dys=None
    sections=[]
    for _ in range(ncmd):
        if off+8>28+cmdbytes:raise ValueError("Truncated load command")
        cmd,length=struct.unpack_from("<II",data,off)
        if length<8 or off+length>28+cmdbytes:raise ValueError("Out-of-bounds load command")
        if cmd==1:
            if length<56:raise ValueError("Short segment")
            name=data[off+8:off+24].split(b"\0",1)[0].decode("ascii")
            vm,vs,fo,fs=struct.unpack_from("<4I",data,off+24)
            nsects=struct.unpack_from("<I",data,off+48)[0]
            if nsects>(length-56)//68 or fo+fs>len(data) or fs>vs:
                raise ValueError("Segment bounds invalid")
            segs[name]=(vm,vs,fo,fs)
            for i in range(nsects):
                sec=off+56+i*68
                addr,size,_,_,_,_,sectflags,reserved1,_=struct.unpack_from("<9I",data,sec+32)
                if (sectflags&255) in (6,7):
                    sections.append((addr,size,reserved1))
        elif cmd==2:
            if length<24:raise ValueError("Short symtab")
            sym=struct.unpack_from("<4I",data,off+8)
        elif cmd==11:
            if length<80:raise ValueError("Short dysymtab")
            dys=struct.unpack_from("<18I",data,off+8)
        off+=length
    if not {"__TEXT","__DATA"}<=set(segs) or sym is None or dys is None:
        raise ValueError("Missing required VM/symbol tables")
    tv,ts,tf,tfs=segs["__TEXT"]
    dv,ds,df,dfs=segs["__DATA"]
    if tv+ts>dv or dv+ds>0xffffffff:raise ValueError("VM overlap/overflow")
    symoff,nsyms,stroff,strsize=sym
    first,nu=dys[4:6]
    indoff,nind=dys[12:14]
    if symoff+nsyms*12>len(data) or stroff+strsize>len(data) or first+nu>nsyms or indoff+nind*4>len(data):
        raise ValueError("Symbol/indirect table exceeds file bounds")
    if len(rbs)<8 or rbs[:4]!=b"RBS1":raise ValueError("Bad RBS1 header")
    count=struct.unpack_from("<I",rbs,4)[0]
    if not count or len(rbs)!=8+count*4 or count>(dv+ds-tv)//4:
        raise ValueError("RBS1 count/size invalid")
    rebases=struct.unpack_from(f"<{count}I",rbs,8)
    if any(x&3 or (i and x<=rebases[i-1]) or not (tv<=x<=tv+ts-4 or dv<=x<=dv+ds-4)
           for i,x in enumerate(rebases)):
        raise ValueError("Unsorted/duplicate/unaligned/out-of-range RBS1")
    # Verify stored relocation targets, not just the slot addresses.
    for va in rebases:
        vm,vs,fo,fs=(tv,ts,tf,tfs) if va<dv else (dv,ds,df,dfs)
        if va+4>vm+fs:
            raise ValueError("RBS1 points to a slot outside the original file data")
        value=struct.unpack_from("<I",data,fo+va-vm)[0]
        if not (tv<=(value&~1)<dv+ds):
            raise ValueError("RBS1 pointer payload outside the mapped guest image")
    rebase_set=set(rebases)
    local=[]
    external=[]
    for addr,size,reserved1 in sections:
        if size&3 or reserved1+size//4>nind:
            raise ValueError("Bad indirect symbol section")
        for j in range(size//4):
            symbol=struct.unpack_from("<I",data,indoff+(reserved1+j)*4)[0]
            if symbol&0x80000000:local.append(addr+j*4)
            elif not symbol&0x40000000:
                if symbol>=nsyms:raise ValueError("Bad indirect symbol index")
                external.append(addr+j*4)
    missing=[x for x in local if x not in rebase_set]
    contaminated=[x for x in external if x in rebase_set]
    if missing or contaminated:
        raise ValueError(f"RBS1 indirect overlap: missed_local={len(missing)} external_rebased={len(contaminated)}")
    return {
        "arch":"ARM","cpu_subtype":subtype,"mach_o_size":len(data),
        "load_commands":ncmd,"text_vm":hex(tv),"data_vm":hex(dv),
        "image_vm_span":dv+ds-tv,"undefined_imports":nu,
        "rbs1_relocations":count,"local_indirect_slots":len(local),
        "external_indirect_slots":len(external),
        "local_slots_all_relocated":not missing,"external_slots_not_relocated":not contaminated
    }

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--ipa",type=Path,required=True)
    ap.add_argument("--rbs1",type=Path,required=True)
    ap.add_argument("--output",type=Path)
    a=ap.parse_args()
    with zipfile.ZipFile(a.ipa) as z:
        data=z.read("Payload/IronMan2.app/IronMan2")
    result=preflight(data,a.rbs1.read_bytes())
    output=json.dumps(result,indent=2)
    if a.output:a.output.write_text(output+"\n")
    print(output)

if __name__=="__main__":main()
