#!/usr/bin/env python3
"""Actual production UIKit CGRect argument IMPs through the production alignment bridge."""
import argparse,json,struct
from pathlib import Path
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
p=argparse.ArgumentParser();p.add_argument('--binary',required=True);p.add_argument('--symbols',required=True);a=p.parse_args()
symbols={}
for line in Path(a.symbols).read_text().splitlines():
 f=line.split()
 if len(f)==3:symbols[f[2]]=int(f[0],16)
code=Path(a.binary).read_bytes();BASE=0x10000;STACK=0x30000;STOP=0x50000;SELF=0x60000;OUT=0x60200
cases=0
for alignment in (0,4):
 u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(BASE,0x20000);u.mem_write(BASE,code)
 u.mem_map(STACK,0x10000);u.mem_map(STOP,0x1000);u.mem_map(SELF,0x1000)
 u.reg_write(UC_ARM_REG_C1_C0_2,0x00f00000);u.reg_write(UC_ARM_REG_FPEXC,0x40000000)
 sp=STACK+0x8000
 def run(address,args,stack=(),misaligned=False,ip=None):
  current_sp=sp+(alignment if misaligned else 0)
  u.reg_write(UC_ARM_REG_SP,current_sp);u.reg_write(UC_ARM_REG_LR,STOP)
  u.reg_write(UC_ARM_REG_R9,0x12345678)
  for r,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):u.reg_write(r,v)
  if ip is not None:u.reg_write(UC_ARM_REG_R12,ip)
  u.mem_write(current_sp,struct.pack('<20I',*(list(stack)+[0]*(20-len(stack)))))
  u.emu_start(address,STOP,count=100000)
  assert u.reg_read(UC_ARM_REG_PC)==STOP and u.reg_read(UC_ARM_REG_SP)==current_sp
  assert u.reg_read(UC_ARM_REG_R9)==0x12345678
  return u.reg_read(UC_ARM_REG_R0)
 assert run(symbols['fixture_setup'],[SELF])==0
 methods=struct.unpack('<4I',u.mem_read(symbols['fixture_methods'],16))
 rects=[(10.,20.,300.,400.),(30.,40.,250.,350.),(5.,6.,200.,300.)]
 for index,rect in enumerate(rects):
  words=struct.unpack('<4I',struct.pack('<4f',*rect))
  # Poison VFP arg registers: a hard-float HFA implementation would read these.
  for r in (UC_ARM_REG_S0,UC_ARM_REG_S1,UC_ARM_REG_S2,UC_ARM_REG_S3):u.reg_write(r,0x7fc00000)
  run(symbols['ml_align_call'],[SELF,0x60100,*words[:2]],words[2:],True,methods[index])
  assert run(symbols['fixture_read'],[OUT])==0
  frame,bounds=struct.unpack('<4f',u.mem_read(OUT,16)),struct.unpack('<4f',u.mem_read(OUT+16,16))
  assert frame==(rect if index<2 else rects[1]),(alignment,index,frame)
  if index==2:assert bounds==rect,(alignment,index,bounds)
  else:assert bounds[2:]==rect[2:],(alignment,index,bounds)
  cases+=1
print(json.dumps({'production_uikit_rect_imp_cases':cases,'physical_vita_verified':False,'original_game_executed':False}))
