#!/usr/bin/env python3
"""Read-only RBS1 safety audit against a user-owned Iron Man 2 IPA."""
import argparse,collections,json,struct,zipfile
p=argparse.ArgumentParser();p.add_argument("ipa");p.add_argument("rbs1");a=p.parse_args()
with zipfile.ZipFile(a.ipa) as z:b=z.read("Payload/IronMan2.app/IronMan2")
segs=[];sects=[];off=28
for _ in range(struct.unpack_from("<I",b,16)[0]):
 cmd,size=struct.unpack_from("<II",b,off)
 if cmd==1:
  name=b[off+8:off+24].split(b"\0")[0].decode();vm,vs,fo,fs,_,_,ns,_=struct.unpack_from("<8I",b,off+24)
  segs.append((name,vm,vs,fo,fs))
  for i in range(ns):
   s=off+56+68*i;sn=b[s:s+16].split(b"\0")[0].decode();addr,sz=struct.unpack_from("<II",b,s+32);sects.append((sn,addr,sz))
 off+=size
r=open(a.rbs1,"rb").read()
assert r[:4]==b"RBS1"
n=struct.unpack_from("<I",r,4)[0];assert len(r)==8+n*4
slots=struct.unpack_from("<"+str(n)+"I",r,8)
lo=min(vm for name,vm,vs,fo,fs in segs if name!="__PAGEZERO")
hi=max(vm+vs for name,vm,vs,fo,fs in segs if name not in ("__LINKEDIT","__PAGEZERO"))
c=collections.Counter();invalid=[]
for va in slots:
 s=next(((name,vm,fo,fs) for name,vm,vs,fo,fs in segs if vm<=va and va+4<=vm+fs),None)
 if not s:invalid.append(hex(va));continue
 name,vm,fo,fs=s;v=struct.unpack_from("<I",b,fo+va-vm)[0]
 c["zero" if v==0 else ("in_image" if lo<=(v&~1)<hi else "out_of_image")]+=1
init=next((set(range(addr,addr+size,4)) for name,addr,size in sects if name=="__mod_init_func"),set())
print(json.dumps({"entries":n,"values":dict(c),"invalid_slots":len(invalid),"missing_constructor_slots":len(init-set(slots))},indent=2))
if invalid or c["zero"] or c["out_of_image"] or len(init-set(slots)):raise SystemExit(1)
