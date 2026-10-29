#!/usr/bin/env python3
"""Host-only contiguous Mach-O image mapping and RBS1 rebase simulation."""
import argparse,json,struct,zipfile,collections
p=argparse.ArgumentParser();p.add_argument('ipa');p.add_argument('rbs1');p.add_argument('--base',type=lambda x:int(x,0),default=0x98001000);a=p.parse_args()
with zipfile.ZipFile(a.ipa) as z:b=z.read('Payload/IronMan2.app/IronMan2')
off=28;segs=[];sects=[]
for _ in range(struct.unpack_from('<I',b,16)[0]):
 cmd,size=struct.unpack_from('<II',b,off)
 if cmd==1:
  name=b[off+8:off+24].split(b'\0')[0].decode();vm,vs,fo,fs,_,_,ns,_=struct.unpack_from('<8I',b,off+24)
  segs.append((name,vm,vs,fo,fs))
  for j in range(ns):
   q=off+56+68*j;sn=b[q:q+16].split(b'\0')[0].decode();addr,sz=struct.unpack_from('<II',b,q+32)
   sects.append((sn,addr,sz))
 off+=size
mapped=[s for s in segs if s[0] in ('__TEXT','__DATA')];lo=min(s[1] for s in mapped);hi=max(s[1]+s[2] for s in mapped)
span=hi-lo;slide=a.base-lo
if a.base+span>2**32:raise ValueError('Image exceeds 32-bit address space')
mem=bytearray(span)
for name,vm,vs,fo,fs in mapped:mem[vm-lo:vm-lo+fs]=b[fo:fo+fs]
r=open(a.rbs1,'rb').read();assert r[:4]==b'RBS1';n=struct.unpack_from('<I',r,4)[0];assert len(r)==8+4*n
slots=struct.unpack_from('<'+str(n)+'I',r,8);seen=set();examples=[];kinds=collections.Counter()
for va in slots:
 if va in seen:raise ValueError('Duplicate rebase slot')
 seen.add(va)
 if not lo<=va<=hi-4 or va%4:raise ValueError('Invalid slot '+hex(va))
 v=struct.unpack_from('<I',mem,va-lo)[0]
 if not lo<=(v&~1)<hi:raise ValueError('Suspicious pointer '+hex(va)+': '+hex(v))
 nv=(v+slide)&0xffffffff
 struct.pack_into('<I',mem,va-lo,nv)
 if len(examples)<5:examples.append(dict(slot=hex(va),before=hex(v),after=hex(nv)))
 kinds['__TEXT' if va<next(s[1] for s in mapped if s[0]=='__DATA') else '__DATA']+=1
init=next((range(addr,addr+size,4) for sn,addr,size in sects if sn=='__mod_init_func'),[])
missing=[hex(x) for x in init if x not in seen]
if missing:raise ValueError('Missing constructors '+str(missing[:3]))
init_values=[struct.unpack_from('<I',mem,va-lo)[0] for va in init]
if not all(a.base <= (v&~1) < a.base+span for v in init_values):raise ValueError('Constructor points outside mapped image')
print(json.dumps(dict(image_va_start=hex(lo),image_va_end=hex(hi),contiguous_bytes=span,runtime_base=hex(a.base),slide=hex(slide),rebases=n,rebases_by_segment=dict(kinds),constructors=len(init_values),first_constructor_runtime=hex(init_values[0]),first_rebases=examples,simulated_only=True),indent=2))
