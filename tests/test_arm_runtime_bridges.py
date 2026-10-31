#!/usr/bin/env python3
"""Execute production C++/math bridges and linked SDK runtime in ARM Unicorn.
Heap, Vita Clib memory primitives and single-thread errno/reent storage use
bounded fixtures; OS TLS is not exercised. No Vita/game execution proof.
"""
import argparse, struct, runpy, math
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.arm_const import *
# Reuse the actual ELF/import-table inspection, including its argument parser.
e=runpy.run_path('tests/test_elf_imports.py')
symbols=e['symbols']; sections=e['sections']; binary=e['b']; read=e['read']; cstring=e['cstring']
u=Uc(UC_ARCH_ARM,UC_MODE_ARM)
alloc=[s for s in sections if s[2]&2 and s[5]]
lo=min(s[3] for s in alloc)&~4095;hi=(max(s[3]+s[5] for s in alloc)+4095)&~4095
u.mem_map(lo,hi-lo)
for s in alloc:
 if s[1]!=8:u.mem_write(s[3],binary[s[4]:s[4]+s[5]])
STACK=0x20000000;HEAP=0x21000000;OBJECT=0x22000000;STOP=0x23000000
for base,size in ((STACK,0x10000),(HEAP,0x100000),(OBJECT,0x10000),(STOP,0x1000)):u.mem_map(base,size)
u.reg_write(UC_ARM_REG_C1_C0_2,0x00f00000);u.reg_write(UC_ARM_REG_FPEXC,0x40000000)
next_alloc=HEAP;live={};calls=0

def hook(uc,addr,size,_):
 global next_alloc
 if addr==(symbols['_Znwj']&~1):
  n=uc.reg_read(UC_ARM_REG_R0);assert 0<n<0x10000
  ptr=next_alloc;next_alloc+=(n+15)&~15;assert next_alloc<HEAP+0x100000
  live[ptr]=n;uc.reg_write(UC_ARM_REG_R0,ptr);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
 elif addr==(symbols['_ZdlPv']&~1):
  ptr=uc.reg_read(UC_ARM_REG_R0)
  if ptr:assert ptr in live;del live[ptr]
  uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
 elif addr in {symbols[n]&~1 for n in ('memcpy','memmove','memset')} :
  dst=uc.reg_read(UC_ARM_REG_R0);src=uc.reg_read(UC_ARM_REG_R1);n=uc.reg_read(UC_ARM_REG_R2)
  assert n<0x10000
  value=bytes([src&255])*n if addr==(symbols['memset']&~1) else bytes(uc.mem_read(src,n))
  if n:uc.mem_write(dst,value)
  uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
 elif addr==(symbols['__getreent']&~1):
  uc.reg_write(UC_ARM_REG_R0,struct.unpack('<I',uc.mem_read(symbols['_impure_ptr'],4))[0])
  uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
 elif addr==(symbols['__errno']&~1):
  uc.reg_write(UC_ARM_REG_R0,OBJECT+0x2000)
  uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
 elif addr==(symbols['fatal_error']&~1):raise AssertionError('ARM bridge rejected operation')
u.hook_add(UC_HOOK_CODE,hook)
def invalid(uc,access,addr,size,value,_):
 raise AssertionError(f'unmapped ARM access {access} {addr:x} PC={uc.reg_read(UC_ARM_REG_PC):x} LR={uc.reg_read(UC_ARM_REG_LR):x}')
u.hook_add(UC_HOOK_MEM_INVALID,invalid)
entries={}
for i in range(struct.unpack('<I',read(symbols['legacy_string_imports_count'],4))[0]):
 n,t,k=struct.unpack('<III',read(symbols['legacy_string_imports']+12*i,12));entries[cstring(n)]=(t,k)

def call(name,args,misalign=0):
 global calls
 target,kind=entries[name if name.startswith('_') else '__'+name];assert kind==0
 sp=STACK+0x8000+misalign
 u.reg_write(UC_ARM_REG_SP,sp);u.reg_write(UC_ARM_REG_LR,STOP);u.reg_write(UC_ARM_REG_R12,target)
 u.reg_write(UC_ARM_REG_R9,0x12345678)
 for reg,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),(args+[0]*4)[:4]):u.reg_write(reg,v)
 words=args[4:]+[0]*16;u.mem_write(sp,struct.pack('<'+'I'*len(words),*words))
 try: u.emu_start(symbols['ml_align_call'],STOP,count=500000)
 except Exception as ex: raise AssertionError(f'{name}: PC={u.reg_read(UC_ARM_REG_PC):x} LR={u.reg_read(UC_ARM_REG_LR):x}') from ex
 assert u.reg_read(UC_ARM_REG_PC)==STOP and u.reg_read(UC_ARM_REG_SP)==sp
 assert u.reg_read(UC_ARM_REG_R9)==0x12345678
 calls+=1;return u.reg_read(UC_ARM_REG_R0)
def pointer(o):return struct.unpack('<I',u.mem_read(o,4))[0]
def content(o):
 p=pointer(o);n,cap,ref=struct.unpack('<IIi',u.mem_read(p-12,12));assert n<=cap
 return bytes(u.mem_read(p,n))
A=OBJECT;B=A+16;C=A+32;TEXT=A+256
u.mem_write(TEXT,b'Iron Man\x00')
for align in (0,4):
 ref=OBJECT+0x1000;u.mem_write(ref,struct.pack('<i',1))
 assert call('ZN9__gnu_cxx18__exchange_and_addEPVii',[ref,0xffffffff],align)==1
 assert call('ZN9__gnu_cxx18__exchange_and_addEPVii',[ref,0xffffffff],align)==0
 assert struct.unpack('<i',u.mem_read(ref,4))[0]==-1
 call('ZNSsC1EPKcRKSaIcE',[A,TEXT,0],align);assert content(A)==b'Iron Man'
 assert struct.unpack('<I',u.mem_read(pointer(A)-12,4))[0]==8
 call('ZNSsC1ERKSs',[B,A],align);assert pointer(A)==pointer(B)
 p=call('ZNSsixEm',[B,0],align);u.mem_write(p,b'i');assert content(A)==b'Iron Man' and content(B)==b'iron Man'
 call('ZNKSs6substrEmm',[C,A,5,0xffffffff],align);assert content(C)==b'Man'
 call('ZNSs6insertEmRKSsmm',[B,4,C,0,0xffffffff],align);assert content(B)==b'ironMan Man'
 assert call('ZNKSs4findEPKcm',[A,TEXT+5,0],align)==5
 assert call('ZNKSs4findEPKcm',[B,TEXT,0],align)==0xffffffff
 call('ZNSsD2Ev',[C],align)
 call('ZNSs9_M_mutateEmmm',[B,4,7,1],align)
 p=call('ZNSsixEm',[B,4],align);u.mem_write(p,b'2');assert content(B)==b'iron2'
 call('ZNSsD2Ev',[A],align);call('ZNSsD2Ev',[B],align)
 u.mem_write(TEXT+100,b'\0');call('ZNSsC1EPKcRKSaIcE',[C,TEXT+100,0],align)
 empty,kind=entries['__ZNSs4_Rep20_S_empty_rep_storageE'];assert kind==1 and pointer(C)==empty+12
 call('ZNSsD2Ev',[C],align);assert not live,live
print(f'PASS: {calls} actual ARM string bridge calls at both stack alignments; SDK COW header12, hidden result, five-argument insert, mutation and data symbol. Heap and Vita Clib memory primitives are synthetic; no device/game execution.')

# Test actual linked SoftFP libm bridges, not host math implementations.
math_count=struct.unpack('<I',read(symbols['math_bridge_imports_count'],4))[0]
assert math_count==23
for i in range(math_count):
 n,t,k=struct.unpack('<III',read(symbols['math_bridge_imports']+12*i,12));entries[cstring(n)]=(t,k)
math_start=calls

def words(x):return list(struct.unpack('<II',struct.pack('<d',x)))
def float_word(x):return struct.unpack('<I',struct.pack('<f',x))[0]
def calculate(name,args,single=False,align=0):
 # Garbage VFP input proves base AAPCS register/stack argument routing.
 for reg in (UC_ARM_REG_S0,UC_ARM_REG_S1,UC_ARM_REG_S2,UC_ARM_REG_S3):u.reg_write(reg,0x7fc00000)
 low=call('_'+name,args,align)
 if single:return struct.unpack('<f',struct.pack('<I',low))[0]
 return struct.unpack('<d',struct.pack('<II',low,u.reg_read(UC_ARM_REG_R1)))[0]
def near(result,expected,single=False):
 assert math.isclose(result,expected,rel_tol=2e-6 if single else 2e-12,abs_tol=1e-7 if single else 1e-14),(result,expected)
for align in (0,4):
 for name,x,expected in [('acos',0.,math.pi/2),('asin',.5,math.pi/6),('atan',1.,math.pi/4),
                         ('ceil',-1.25,-1.),('exp',1.,math.e),('log',math.e,1.),
                         ('log2',8.,3.),('tan',math.pi/4,1.)]:
  near(calculate(name,words(x),align=align),expected)
 for name,x,expected in [('asinf',.5,math.pi/6),('atanf',1.,math.pi/4),('ceilf',-1.25,-1.),
                         ('coshf',0.,1.),('roundf',2.5,3.),('roundf',-2.5,-3.),
                         ('sinhf',0.,0.),('tanhf',0.,0.)]:
  near(calculate(name,[float_word(x)],True,align),expected,True)
 near(calculate('atan2f',[float_word(1.),float_word(-1.)],True,align),3*math.pi/4,True)
 near(calculate('fmodf',[float_word(-5.5),float_word(2.)],True,align),-1.5,True)
 near(calculate('fmax',words(-4.)+words(2.),align=align),2.)
 near(calculate('fmin',words(-4.)+words(2.),align=align),-4.)
 near(calculate('fmax',words(math.nan)+words(2.),align=align),2.)
 near(calculate('fmin',words(2.)+words(math.nan),align=align),2.)
 assert math.isnan(calculate('acos',words(2.),align=align))
 assert struct.unpack('<i',u.mem_read(OBJECT+0x2000,4))[0]==33
 near(calculate('frexp',words(12.)+[OBJECT+64],align=align),.75)
 assert struct.unpack('<i',u.mem_read(OBJECT+64,4))[0]==4
 near(calculate('modf',words(-3.25)+[OBJECT+72],align=align),-.25)
 assert struct.unpack('<d',u.mem_read(OBJECT+72,8))[0]==-3.
 u.mem_write(TEXT,b' -12.5tail\0')
 near(calculate('strtod',[TEXT,OBJECT+64],align=align),-12.5)
 assert pointer(OBJECT+64)==TEXT+6
 near(calculate('atof',[TEXT],align=align),-12.5)
 assert math.copysign(1.,calculate('roundf',[0x80000000],True,align))==-1.
print(f'PASS: {calls-math_start} production ARM math calls, both stack alignments, poisoned VFP args, r0/r1 double returns, out-pointers, NaN, signed zero, rounding and strtod end-pointer.')
