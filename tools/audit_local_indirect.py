#!/usr/bin/env python3
"""Audit whether Mach-O local indirect pointer slots are already in RBS1."""
import argparse,json,struct,zipfile
p=argparse.ArgumentParser()
p.add_argument("ipa");p.add_argument("rbs1",nargs="+")
a=p.parse_args()
with zipfile.ZipFile(a.ipa) as z:
    b=z.read("Payload/IronMan2.app/IronMan2")
o=28;sects=[];dys=None
for _ in range(struct.unpack_from("<I",b,16)[0]):
    cmd,size=struct.unpack_from("<II",b,o)
    if cmd==1:
        for j in range(struct.unpack_from("<I",b,o+48)[0]):
            q=o+56+j*68
            addr,sz=struct.unpack_from("<II",b,q+32)
            flags,r1=struct.unpack_from("<II",b,q+56)
            sects.append((addr,sz,flags&255,r1))
    elif cmd==11:dys=struct.unpack_from("<18I",b,o+8)
    o+=size
if dys is None:raise SystemExit("Missing LC_DYSYMTAB")
indoff,nind=dys[12:14]
local=set();external=set();absolute=set()
for addr,sz,kind,r1 in sects:
    if kind not in (6,7):continue
    for j in range(sz//4):
        index=struct.unpack_from("<I",b,indoff+(r1+j)*4)[0]
        slot=addr+j*4
        if index&0x80000000:local.add(slot)
        elif index&0x40000000:absolute.add(slot)
        else:external.add(slot)
out={}
for path in a.rbs1:
    r=open(path,"rb").read()
    if r[:4]!=b"RBS1":raise ValueError("Not RBS1: "+path)
    n=struct.unpack_from("<I",r,4)[0]
    if len(r)!=8+4*n:raise ValueError("Invalid RBS1 length")
    slots=set(struct.unpack_from("<"+str(n)+"I",r,8))
    out[path]={"total":n,"local_slots":len(local),"local_in_rbs1":len(local&slots),"local_missing":len(local-slots),"external_in_rbs1":len(external&slots),"absolute_in_rbs1":len(absolute&slots)}
print(json.dumps(out,indent=2))
