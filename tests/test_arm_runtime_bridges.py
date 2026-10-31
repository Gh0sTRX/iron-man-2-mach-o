#!/usr/bin/env python3
"""Execute production C++/math bridges and linked SDK runtime in ARM Unicorn.
Heap, Vita Clib memory primitives and single-thread errno/reent storage use
bounded fixtures; OS TLS and mutex scheduling are not exercised. No Vita/game execution proof.
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

def allocate(n):
 global next_alloc
 assert 0<=n<0x10000
 n=max(n,1);ptr=next_alloc;next_alloc+=(n+15)&~15;assert next_alloc<HEAP+0x100000
 live[ptr]=n;u.mem_write(ptr,bytes(n));return ptr

def hook(uc,addr,size,_):
 def matches(*names):return any(addr==(symbols[n]&~1) for n in names if n in symbols)
 def arg(n):return uc.reg_read((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2)[n])
 def returned(value=None):
  if value is not None:uc.reg_write(UC_ARM_REG_R0,value)
  uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
 if matches('_Znwj','malloc','_malloc_r','calloc','_calloc_r'):
  if matches('calloc'):n=arg(0)*arg(1)
  elif matches('_calloc_r'):n=arg(1)*arg(2)
  else:n=arg(1 if matches('_malloc_r') else 0)
  returned(allocate(n))
 elif matches('realloc','_realloc_r'):
  offset=int(matches('_realloc_r'));ptr=arg(offset);n=arg(offset+1)
  result=allocate(n)
  if ptr:
   assert ptr in live
   uc.mem_write(result,bytes(uc.mem_read(ptr,min(live[ptr],n))))
   del live[ptr]
  returned(result)
 elif matches('_ZdlPv','free','_free_r'):
  ptr=arg(int(matches('_free_r')))
  if ptr:assert ptr in live;del live[ptr]
  returned()
 elif matches('pthread_mutex_lock','pthread_mutex_unlock'):
  returned(0) # Single-thread fixture: no OS mutex/scheduling coverage.
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

# Original ARM node layouts: list links +0/+4, tree color/parent/left/right
# +0/+4/+8/+12. Payload belongs to the guest and must remain untouched.
container_count=struct.unpack('<I',read(symbols['legacy_container_imports_count'],4))[0]
assert container_count==8
for i in range(container_count):
 n,t,k=struct.unpack('<III',read(symbols['legacy_container_imports']+12*i,12));entries[cstring(n)]=(t,k)
container_start=calls
LIST_HOOK='ZNSt15_List_node_base4hookEPS_'
LIST_UNHOOK='ZNSt15_List_node_base6unhookEv'
LIST_TRANSFER='ZNSt15_List_node_base8transferEPS_S0_'
INSERT='ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_'
ERASE='ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_'
NEXT='ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base'
NEXTC='ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base'
PREV='ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base'
GUARD=0xa57ead01

def put(p,*v):u.mem_write(p,struct.pack('<'+'I'*len(v),*v))
def get(p,n):return struct.unpack('<'+'I'*n,u.mem_read(p,4*n))

def ring(h,expected):
 current=pointer(h);previous=h;seen=[]
 while current!=h:
  assert current not in seen and len(seen)<=4
  assert pointer(current+4)==previous
  seen.append(current);previous=current;current=pointer(current)
 assert pointer(h+4)==previous and seen==expected

def check_tree(h,present,nodes):
 color,root,left,right=get(h,4);assert color==0
 seen=set();ordered=[]
 def walk(p,parent):
  if not p:return 1
  assert p in nodes and p not in seen;seen.add(p)
  c,up,lo,hi,key,guard=get(p,6);assert up==parent and c in (0,1) and guard==GUARD
  assert key==nodes.index(p)
  if c==0:
   assert not lo or pointer(lo)==1
   assert not hi or pointer(hi)==1
  lh=walk(lo,p);ordered.append(key);rh=walk(hi,p);assert lh==rh
  return lh+(c==1)
 if root:assert pointer(root)==1
 walk(root,h);assert ordered==sorted(present)
 if present:assert left==nodes[min(present)] and right==nodes[max(present)]
 else:assert root==0 and left==h and right==h
 for key,p in enumerate(nodes):assert get(p+16,4)==(key,GUARD,GUARD,GUARD)

import random
for align in (0,4):
 L=OBJECT+0x3000;H=L;J=L+32;nodes=[L+64+i*32 for i in range(4)]
 for p in [H,J,*nodes]:put(p,p,p,GUARD,GUARD)
 for p in nodes:call(LIST_HOOK,[p,H],align)
 ring(H,nodes);call(LIST_UNHOOK,[nodes[1]],align);ring(H,[nodes[0],nodes[2],nodes[3]])
 call(LIST_TRANSFER,[J,nodes[2],H],align);ring(H,[nodes[0]]);ring(J,nodes[2:])
 call(LIST_TRANSFER,[H,J,J],align);ring(H,[nodes[0]]);ring(J,nodes[2:])
 call(LIST_TRANSFER,[H,pointer(J),J],align);ring(H,[nodes[0],nodes[2],nodes[3]]);ring(J,[])
 call(LIST_TRANSFER,[H,pointer(H),H],align);ring(H,[nodes[0],nodes[2],nodes[3]])
 for p in [H,J,*nodes]:assert get(p+8,2)==(GUARD,GUARD)
 H=OBJECT+0x4000;nodes=[H+32+i*32 for i in range(31)]
 for key,p in enumerate(nodes):put(p,0,0,0,0,key,GUARD,GUARD,GUARD)
 rng=random.Random(0x1a02+align);keys=list(range(len(nodes)))
 for deletion in ('ascending','descending','random'):
  put(H,0,0,H,H);present=set();rng.shuffle(keys)
  for key in keys:
   parent=H;p=pointer(H+4);left=1
   while p:
    parent=p;left=int(key<pointer(p+16));p=pointer(p+(8 if left else 12))
   call(INSERT,[0x100|left,nodes[key],parent,H],align);present.add(key);check_tree(H,present,nodes)
  p=pointer(H+8)
  for key in range(len(nodes)):
   assert p==nodes[key]
   nxt=call(NEXT,[p],align);assert nxt==call(NEXTC,[p],align);p=nxt
  assert p==H;p=call(PREV,[H],align)
  for key in reversed(range(len(nodes))):
   assert p==nodes[key]
   if key:p=call(PREV,[p],align)
  order=sorted(present,reverse=deletion=='descending')
  if deletion=='random':rng.shuffle(order)
  for key in order:
   assert call(ERASE,[nodes[key],H],align)==nodes[key]
   present.remove(key);check_tree(H,present,nodes)
print(f'PASS: {calls-container_start} production ARM container calls; 8/16-byte nodes, splice, red/black invariants, payload preservation, sorted/const/reverse iteration and one-byte bool normalization at both stack alignments.')

cg_count=struct.unpack('<I',read(symbols['cg_geometry_imports_count'],4))[0]
assert cg_count==5
for i in range(cg_count):
 n,t,k=struct.unpack('<III',read(symbols['cg_geometry_imports']+12*i,12));entries[cstring(n)]=(t,k)
cg_start=calls
OUT=OBJECT+0x6000
for align in (0,4):
 for angle in (0.,math.pi/2,-math.pi/2,math.pi):
  put(OUT-4,GUARD,*([GUARD]*6),GUARD)
  for reg in (UC_ARM_REG_S0,UC_ARM_REG_S1,UC_ARM_REG_S2,UC_ARM_REG_S3):u.reg_write(reg,0x7fc00000)
  call('_CGAffineTransformMakeRotation',[OUT,float_word(angle)],align)
  result=struct.unpack('<6f',u.mem_read(OUT,24))
  for actual,expected in zip(result,(math.cos(angle),math.sin(angle),-math.sin(angle),math.cos(angle),0.,0.)):
   near(actual,expected,True)
  assert pointer(OUT-4)==GUARD and pointer(OUT+24)==GUARD
 for x,y in ((2.,3.),(-2.,.5),(0.,-1.)):
  put(OUT-4,GUARD,*([GUARD]*6),GUARD)
  for reg in (UC_ARM_REG_S0,UC_ARM_REG_S1,UC_ARM_REG_S2,UC_ARM_REG_S3):u.reg_write(reg,0x7fc00000)
  call('_CGAffineTransformMakeScale',[OUT,float_word(x),float_word(y)],align)
  assert struct.unpack('<6f',u.mem_read(OUT,24))==(x,0.,0.,y,0.,0.)
  assert pointer(OUT-4)==GUARD and pointer(OUT+24)==GUARD
 for rect,dx,dy in (((10.,20.,300.,400.),-5.,2.),((-10.,-20.,0.,-8.),.5,-1.5)):
  put(OUT-4,GUARD,*([GUARD]*4),GUARD)
  for reg in (UC_ARM_REG_S0,UC_ARM_REG_S1,UC_ARM_REG_S2,UC_ARM_REG_S3):u.reg_write(reg,0x7fc00000)
  args=[OUT]+[float_word(x) for x in (*rect,dx,dy)]
  call('_CGRectOffset',args,align)
  assert struct.unpack('<4f',u.mem_read(OUT,16))==(rect[0]+dx,rect[1]+dy,rect[2],rect[3])
  assert pointer(OUT-4)==GUARD and pointer(OUT+16)==GUARD
 for name,size in (('_CGPointZero',8),('_CGRectZero',16)):
  address,kind=entries[name];assert kind==1 and bytes(u.mem_read(address,size))==bytes(size)
print(f'PASS: {calls-cg_start} production CoreGraphics ARM aggregate calls; hidden outputs, split rectangle args, poisoned VFP args, rotation/scale/offset and bounds guards, both stack alignments; no GPU/view transform claim.')

# Real ARM CF-array + managed-runtime integration. Only allocator/memory/OS
# synchronization primitives above are substituted; class/object/CF code is real.
for table in ('cfarray_imports','cf_memory_imports'):
 n=struct.unpack('<I',read(symbols[table+'_count'],4))[0]
 for i in range(n):
  name,t,k=struct.unpack('<III',read(symbols[table]+12*i,12));entries[cstring(name)]=(t,k)
for name in ('im2_objc_register_class','im2_cfarrays_init','im2_objc_alloc','im2_objc_retain_count'):
 entries['__'+name]=(symbols[name],0)
cf_start=calls
u.mem_write(TEXT,b'NSObject\0')
root=call('im2_objc_register_class',[TEXT,0,4,0,0,0,0]);assert root
assert call('im2_cfarrays_init',[root])==0
registry_heap=dict(live)
for align in (0,4):
 default,kind=entries['_kCFAllocatorDefault'];assert kind==1 and pointer(default)==0
 a=call('_CFArrayCreateMutable',[pointer(default),0,0],align);assert a
 assert call('_CFArrayGetCount',[a],align)==0
 assert call('im2_objc_retain_count',[a],align)==1
 value=call('im2_objc_alloc',[root,0],align);assert value
 call('_CFArrayAppendValue',[a,value],align);call('_CFArrayAppendValue',[a,value],align)
 assert call('im2_objc_retain_count',[value],align)==1
 assert call('_CFArrayGetValueAtIndex',[a,0],align)==value
 assert call('_CFArrayGetValueAtIndex',[a,1],align)==value
 call('_CFRelease',[value],align);assert call('im2_objc_retain_count',[value],align)==0
 call('_CFArrayRemoveAllValues',[a],align) # old pointers are dangling, not released
 for v in range(20):call('_CFArrayAppendValue',[a,0xdead0000+v],align)
 assert call('_CFArrayGetCount',[a],align)==20
 for v in range(20):assert call('_CFArrayGetValueAtIndex',[a,v],align)==0xdead0000+v
 assert call('_CFRetain',[a],align)==a
 call('_CFRelease',[a],align);assert call('im2_objc_retain_count',[a],align)==1
 call('_CFRelease',[a],align);assert call('im2_objc_retain_count',[a],align)==0
 assert live==registry_heap, (live,registry_heap)
print(f'PASS: {calls-cf_start} production ARM CF/runtime calls, real class registration and object lifetime; borrowed duplicate/dangling/raw values, growth, Create/Get rules, shared retain/release and complete object/array heap cleanup. OS locks/allocators are fixtures.')
