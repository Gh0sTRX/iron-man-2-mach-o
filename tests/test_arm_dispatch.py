#!/usr/bin/env python3
"""Execute actual production ARM glue; this is CPU emulation, not a Vita hardware test."""
import argparse
import json
from pathlib import Path
import struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import *

p=argparse.ArgumentParser();p.add_argument('--binary',required=True);p.add_argument('--symbols',required=True);a=p.parse_args()
symbols={}
for line in Path(a.symbols).read_text().splitlines():
    fields=line.split()
    if len(fields)==3:
        symbols[fields[2]]=int(fields[0],16)
code=Path(a.binary).read_bytes()
BASE=0x10000;STACK=0x30000;STOP=0x50000;SELF=0x60000;SEL=0x60100;RESULT=0x60200
checks=0
for misalignment in (0,4):
    for mode in ('word','stret','nil','nil_stret','arm_guest','thumb_guest','guest6'):
        uc=Uc(UC_ARCH_ARM,UC_MODE_ARM)
        uc.mem_map(BASE,0x10000);uc.mem_write(BASE,code)
        uc.mem_map(STACK,0x10000);uc.mem_map(STOP,0x1000);uc.mem_map(SELF,0x1000)
        sp=STACK+0x8000+misalignment
        uc.reg_write(UC_ARM_REG_SP,sp);uc.reg_write(UC_ARM_REG_LR,STOP)
        uc.reg_write(UC_ARM_REG_R9,0x12345678)
        preserved={}
        for i,reg in enumerate((UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R10,UC_ARM_REG_R11)):
            preserved[reg]=0xa000+i;uc.reg_write(reg,0xa000+i)
        uc.mem_write(sp,struct.pack('<20I',*range(30,50)))
        uc.reg_write(UC_ARM_REG_R0,SELF);uc.reg_write(UC_ARM_REG_R1,SEL)
        uc.reg_write(UC_ARM_REG_R2,10);uc.reg_write(UC_ARM_REG_R3,20)
        entry='im2_objc_msgSend_entry'
        if mode in ('stret','nil_stret'):
            entry='im2_objc_msgSend_stret_entry'
            uc.reg_write(UC_ARM_REG_R0,RESULT);uc.reg_write(UC_ARM_REG_R1,SELF);uc.reg_write(UC_ARM_REG_R2,SEL)
        if mode=='nil':uc.reg_write(UC_ARM_REG_R0,0)
        if mode=='nil_stret':uc.reg_write(UC_ARM_REG_R1,0)
        if mode.endswith('guest') or mode=='guest6':
            # Host entry must be AAPCS-aligned, regardless of guest tests above.
            if misalignment:continue
            entry='call_guest6' if mode=='guest6' else 'call_guest'
            target='test_thumb_guest' if mode=='thumb_guest' else ('test_arm_guest6' if mode=='guest6' else 'test_arm_guest')
            uc.reg_write(UC_ARM_REG_R0,symbols[target] | (1 if mode=='thumb_guest' else 0))
            uc.reg_write(UC_ARM_REG_R1,1);uc.reg_write(UC_ARM_REG_R2,2);uc.reg_write(UC_ARM_REG_R3,3)
            uc.mem_write(sp,struct.pack('<3I',4,5,6))
        native_calls=[]
        def hook(uc,addr,size,data):
            if addr in (symbols['test_word_imp'],symbols['test_struct_imp']):
                native_calls.append(addr)
                assert uc.reg_read(UC_ARM_REG_SP)%8==0
                # r0-r3 and guest stack words reach the selected IMP untouched.
                if mode=='word':
                    assert [uc.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]==[SELF,SEL,10,20]
                else:
                    assert [uc.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]==[RESULT,SELF,SEL,20]
                assert struct.unpack('<I',uc.mem_read(uc.reg_read(UC_ARM_REG_SP),4))[0]==30
        uc.hook_add(UC_HOOK_CODE,hook)
        uc.emu_start(symbols[entry],STOP,count=1000)
        assert uc.reg_read(UC_ARM_REG_PC)==STOP
        assert uc.reg_read(UC_ARM_REG_SP)==sp
        assert uc.reg_read(UC_ARM_REG_R9)==0x12345678
        for reg,value in preserved.items():assert uc.reg_read(reg)==value
        if mode=='word':assert uc.reg_read(UC_ARM_REG_R0)==60
        if mode=='stret':assert bytes(uc.mem_read(RESULT,8))==struct.pack('<2I',20,30)
        if mode in ('nil','nil_stret'):assert not native_calls
        if mode=='nil':assert uc.reg_read(UC_ARM_REG_R0)==0 and uc.reg_read(UC_ARM_REG_R1)==0
        if mode=='arm_guest':assert uc.reg_read(UC_ARM_REG_R0)==10
        if mode=='guest6':assert uc.reg_read(UC_ARM_REG_R0)==21
        if mode=='thumb_guest':assert uc.reg_read(UC_ARM_REG_R0)==3
        checks+=1
print(json.dumps(dict(cases=checks,production_assembly_executed=True,physical_vita_verified=False,original_game_executed=False)))
