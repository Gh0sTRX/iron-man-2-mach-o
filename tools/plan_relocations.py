#!/usr/bin/env python3
"""Validate ARM Mach-O external relocations; never modify original binary."""
import argparse, collections, json, struct, zipfile

def audit(b):
    if b[:4] != b'\xce\xfa\xed\xfe': raise ValueError('Expected little-endian 32-bit Mach-O')
    _,cpu,subtype,kind,ncmds,cmdsize,_ = struct.unpack_from('<IiiIIII',b)
    off=28; segments=[]; sym=None; dys=None
    if off+cmdsize>len(b): raise ValueError('Invalid command region')
    for _ in range(ncmds):
        cmd,size=struct.unpack_from('<II',b,off)
        if size<8 or off+size>len(b):raise ValueError('Invalid command size')
        if cmd==1:
            name=b[off+8:off+24].split(b'\0')[0].decode('ascii')
            vm,vs,fo,fs=struct.unpack_from('<IIII',b,off+24)
            segments.append(dict(name=name,vm=vm,size=vs,file=fo,filesize=fs))
        if cmd==2:sym=struct.unpack_from('<IIII',b,off+8)
        if cmd==11:dys=struct.unpack_from('<18I',b,off+8)
        off+=size
    if not sym or not dys:raise ValueError('Missing tables')
    symoff,nsyms,stroff,strsize=sym
    first,count=dys[4:6];reloff,nrel=dys[14:16]
    if symoff+nsyms*12>len(b) or stroff+strsize>len(b) or reloff+nrel*8>len(b):raise ValueError('Invalid tables')
    cats=collections.Counter(); types=collections.Counter(); addends=collections.Counter();bad=[];examples=[]
    for i in range(nrel):
        address,info=struct.unpack_from('<II',b,reloff+i*8)
        index=info&0xffffff;pc=(info>>24)&1;length=(info>>25)&3;ext=(info>>27)&1;typ=(info>>28)&15
        types[(typ,pc,length,ext)]+=1
        if index>=nsyms:bad.append((i,'symbol index'));continue
        target=next((s for s in segments if s['vm']<=address<s['vm']+s['filesize']),None)
        if not target or address+4>target['vm']+target['filesize'] or address%4:bad.append((i,'target'));continue
        filepos=target['file']+(address-target['vm'])
        if filepos+4>len(b):bad.append((i,'file bounds'));continue
        ntype=b[symoff+index*12+4]
        category='dynamic_undefined' if first<=index<first+count else ('other_undefined' if (ntype&0xe)==0 else 'defined_or_other')
        cats[category]+=1
        addends['zero' if struct.unpack_from('<I',b,filepos)[0]==0 else 'nonzero']+=1
        if len(examples)<5:examples.append(dict(target=hex(address),symbol_index=index,kind=category))
    return dict(cpu=cpu,subtype=subtype,filetype=kind,relocations=nrel,classes=dict(cats),types=[dict(type=k[0],pcrel=k[1],length=k[2],external=k[3],count=v) for k,v in sorted(types.items())],stored_values=dict(addends),bad_count=len(bad),bad_examples=bad[:10],examples=examples)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('ipa');p.add_argument('--entry',default='Payload/IronMan2.app/IronMan2');a=p.parse_args()
    with zipfile.ZipFile(a.ipa) as z: b=z.read(a.entry)
    print(json.dumps(audit(b),indent=2))
