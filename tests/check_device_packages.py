#!/usr/bin/env python3
"""Inspect built VPK payload/SFO and ELF symbols; not a device behavior test."""
import struct
import subprocess
import sys
import zipfile
from pathlib import Path

def sfo(data):
    magic, version, keys, values, count=struct.unpack_from('<5I',data)
    assert magic==0x46535000
    out={}
    for i in range(count):
        key,fmt,length,capacity,offset=struct.unpack_from('<HHIII',data,20+16*i)
        name=data[keys+key:].split(b'\0',1)[0].decode()
        out[name]=data[values+offset:values+offset+length].rstrip(b'\0').decode() if fmt==0x204 else None
    return out

base=Path(sys.argv[1])
for kind,title,name,elf in (
    ('vitagl','IM2G00001','IM2 VitaGL Diagnostic',base/'im2_vitagl_environment_probe'),
    ('vm','IM2V00001','IM2 VM Cache Diagnostic',base/'im2_vm_diagnostic')):
    with zipfile.ZipFile(base/f'im2-{kind}-diagnostic.vpk') as package:
        files={p.filename for p in package.infolist() if not p.is_dir()}
        assert files=={'eboot.bin','sce_sys/param.sfo'},files
        metadata=sfo(package.read('sce_sys/param.sfo'))
        assert metadata['TITLE_ID']==title and metadata['TITLE']==name,metadata
        assert package.read('eboot.bin')[:4]==b'SCE\0'
    symbols=subprocess.check_output(['arm-vita-eabi-nm',str(elf)],text=True)
    for line in symbols.splitlines():
        fields=line.split()
        assert not (len(fields)==2 and fields[0]=='U'),line
        if len(fields)==3:
            assert fields[2] not in {'macho_load','macho_run_inits','call_guest','objc_init'},line
    print(f'PASS: {kind} diagnostic identity, SELF, asset-free allowlist and absence of guest loader symbols. Device NOT tested.')
