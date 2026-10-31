#!/usr/bin/env python3
"""Execute production C++/math bridges and linked SDK runtime in ARM Unicorn.
Heap, Vita Clib memory primitives and single-thread errno/reent storage use
bounded fixtures; OS TLS and mutex scheduling are not exercised. GLES tests intercept VitaGL
state entrypoints to check ABI forwarding, not GPU state or drawing. No Vita/game execution proof.
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

gles_capture={};gles_seen=[]

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
 if addr in gles_capture:
  name,ni,nf,vector=gles_capture[addr]
  ints=tuple(uc.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)[:ni])
  floats=tuple(uc.reg_read(UC_ARM_REG_S0+i) for i in range(nf))
  values=()
  if vector:
   pointer_arg,n=vector
   if name=='glLightfv':n=1 if ints[1] in (0x1207,0x1208,0x1209) else 4
   values=tuple(struct.unpack('<'+'I'*n,uc.mem_read(ints[pointer_arg],4*n)))
  gles_seen.append((name,ints,floats,values));returned(0)
 elif matches('_Znwj','malloc' ,'_malloc_r','calloc','_calloc_r'):
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

# Check production base-AAPCS wrappers at actual hard-float VitaGL boundaries.
# The GPU implementation is deliberately not run; callbacks capture call values.
gf_count=struct.unpack('<I',read(symbols['gles_float_imports_count'],4))[0]
assert gf_count==19
for table in ('gles_float_imports','gles_imports'):
 n=struct.unpack('<I',read(symbols[table+'_count'],4))[0]
 for i in range(n):
  name,t,k=struct.unpack('<III',read(symbols[table]+12*i,12));entries[cstring(name)]=(t,k)
for name,ni,nf,vec in (
 ('glAlphaFunc',1,1,None),('glClearColor',0,4,None),('glClearDepthf',0,1,None),
 ('glDepthRangef',0,2,None),('glFogf',1,1,None),('glFrustumf',0,6,None),('glOrthof',0,6,None),
 ('glRotatef',0,4,None),('glScalef',0,3,None),('glTranslatef',0,3,None),
 ('glMaterialf',2,1,None),('glLightfv',3,0,(2,4)),('glFogfv',2,0,(1,4)),
 ('glLightModelfv',2,0,(1,4)),('glLoadMatrixf',1,0,(0,16)),
 ('glMaterialfv',3,0,(2,4)),('glMultMatrixf',1,0,(0,16)),
 ('glClearColorx',4,0,None),('glGetError',0,0,None)):
 gles_capture[symbols[name]&~1]=(name,ni,nf,vec)
gles_start=calls
V=OBJECT+0x7000
matrix=[float_word(v/4-2) for v in range(16)]
put(V,*matrix)
for align in (0,4):
 def state(name,args,backend,ints=(),floats=(),values=()):
  previous=len(gles_seen)
  for r in range(8):u.reg_write(UC_ARM_REG_S0+r,0x7fc00000)
  call('_'+name,args,align)
  assert len(gles_seen)==previous+1
  assert gles_seen[-1]==(backend,tuple(ints),tuple(floats),tuple(values)),(name,gles_seen[-1],ints,floats,values)
 for name,ints,params in (
  ('glAlphaFunc',[0x204],[.25]),('glClearColor',[],[.125,.25,.5,1.]),
  ('glClearDepthf',[],[.75]),('glDepthRangef',[],[.125,.875]),('glFogf',[0xb62],[.5]),
  ('glFrustumf',[],[-2.,3.,-4.,5.,.25,200.]),('glOrthof',[],[-1.,10.,-2.,20.,-5.,50.]),
  ('glRotatef',[],[45.,0.,1.,0.]),('glScalef',[],[2.,-.5,3.]),
  ('glTranslatef',[],[-10.,20.,.25]),('glMaterialf',[0x408,0x1601],[32.])):
  values=[float_word(v) for v in params]
  state(name,ints+values,name,ints,values)
 for pname in (0x1207,0x1208,0x1209):
  # Scalar float r2 must become a pointer to exactly one real float, not an
  # address misread from the original float bits or an undersized vector.
  value=float_word(.125)
  before=len(gles_seen);call('_glLightf',[0x4002,pname,value],align)
  assert len(gles_seen)==before+1
  name,ints,floats,vector=gles_seen[-1]
  assert name=='glLightfv' and ints[:2]==(0x4002,pname) and floats==() and vector==(value,)
 # Vector input address is passed unchanged.
 state('glLightfv',[0x4001,0x1203,V],'glLightfv',[0x4001,0x1203,V],(),matrix[:4])
 # Valid nonnegative attenuation input, using a separate slot.
 put(V+80,float_word(.5));state('glLightfv',[0x4001,0x1208,V+80],'glLightfv',[0x4001,0x1208,V+80],(),[float_word(.5)])
 state('glFogfv',[0xb66,V],'glFogfv',[0xb66,V],(),matrix[:4])
 state('glLightModelfv',[0xb53,V],'glLightModelfv',[0xb53,V],(),matrix[:4])
 state('glMaterialfv',[0x408,0x1200,V],'glMaterialfv',[0x408,0x1200,V],(),matrix[:4])
 state('glLoadMatrixf',[V],'glLoadMatrixf',[V],(),matrix)
 state('glMultMatrixf',[V],'glMultMatrixf',[V],(),matrix)
 fixed=[0xffff0000,0,0x8000,0x10000]
 state('glClearColorx',fixed,'glClearColorx',fixed)
 # Guards must stop before reaching a backend that could index out of bounds
 # or read four floats from a one-float scalar temporary.
 for name,args,error in (
  ('glLightf',[0x4000,0x1200,float_word(1.)],0x500),
  ('glLightf',[0x3fff,0x1209,float_word(1.)],0x500),
  ('glLightf',[0x4008,0x1209,float_word(1.)],0x500),
  ('glLightf',[0x4000,0x1209,float_word(-.5)],0x501),
  ('glLightfv',[0xffffffff,0x1200,V],0x500),
  ('glLightfv',[0x4000,0x1203,0],0x501),
  ('glLightfv',[0x4000,0x1204,V],0x500),
  ('glLightfv',[0x4000,0x1208,V],0x501)):
  before=len(gles_seen);call('_'+name,args,align);assert len(gles_seen)==before
  assert call('_glGetError',[],align)==error
  assert call('_glGetError',[],align)==0
 # First error is preserved until read; next GetError falls through to backend.
 call('_glLightf',[0x4009,0x1209,0],align)
 call('_glLightfv',[0x4000,0x1203,0],align)
 assert call('_glGetError',[],align)==0x500
 assert call('_glGetError',[],align)==0
print(f'PASS: {calls-gles_start} GLES ARM state/error calls, 19 import entries, register/stack-to-VFP forwarding, scalar/vector lighting, matrix pointers and first-error guards at both alignments. VitaGL entrypoints captured; no GPU output verified.')

# Actual guest draw/array imports -> production converter -> captured VitaGL.
# This tests the CPU path and state restoration, not rasterization or GPU copies.
for name in ('gles_client_arrays_init','gles_client_arrays_restore','gles_get_draw_calls'):
 entries['__'+name]=(symbols[name],0)
array_native={symbols[name]&~1:name for name in ('glVertexPointer','glColorPointer','glNormalPointer',
 'glTexCoordPointer','glClientActiveTexture','glEnableClientState','glDisableClientState',
 'glDrawArrays','glDrawElements')}
array_state={};array_enabled=set();array_unit=0;array_draws=[]

def array_hook(uc,address,size,_):
 global array_unit
 if address not in array_native:return
 name=array_native[address]
 args=[uc.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
 def slot(cap):return {0x8074:0,0x8076:1,0x8075:2,0x8078:3+array_unit}[cap]
 if name=='glClientActiveTexture':array_unit=args[0]-0x84c0;assert array_unit in (0,1)
 elif name in ('glEnableClientState','glDisableClientState'):
  if name=='glEnableClientState':array_enabled.add(slot(args[0]))
  else:array_enabled.discard(slot(args[0]))
 elif name.endswith('Pointer'):
  if name=='glNormalPointer':s=2;n=3;typ,stride,p=args[:3]
  else:
   s=0 if name=='glVertexPointer' else 1 if name=='glColorPointer' else 3+array_unit
   n,typ,stride,p=args
  array_state[s]=(n,typ,stride,p)
 else:
  if name=='glDrawElements':
   mode,count,typ,p=args
   assert typ in (0x1401,0x1403)
   indices=list(uc.mem_read(p,count)) if typ==0x1401 else list(struct.unpack('<'+'H'*count,uc.mem_read(p,count*2)))
  else:mode,first,count=args[:3];indices=list(range(first,first+count))
  attributes={}
  for s in array_enabled:
   n,typ,stride,p=array_state[s];assert p and typ in (0x1406,0x1401)
   bpc=4 if typ==0x1406 else 1;step=stride or bpc*n
   attributes[s]=[tuple(struct.unpack('<'+'f'*n,uc.mem_read(p+i*step,n*4))) if bpc==4 else tuple(uc.mem_read(p+i*step,n)) for i in indices]
  array_draws.append((name,mode,indices,attributes))
 uc.reg_write(UC_ARM_REG_R0,0);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
u.hook_add(UC_HOOK_CODE,array_hook)
array_start=calls
assert call('gles_client_arrays_init',[])==0
V=OBJECT+0x8000;C=V+64;N=C+64;T0=N+64;T1=T0+64;IDX=T1+64;OUT=IDX+32
for i in range(3):
 u.mem_write(V+10*i,struct.pack('<ii',(i+1)*65536,-(i+1)*32768))
put(C,*([65536,32768,0,65536]*3))
put(N,*([0,0,65536]*3));put(T0,*([0,65536]*3));put(T1,*([32768,16384]*3))
u.mem_write(IDX,bytes([2,0,2]));u.mem_write(IDX+9,struct.pack('<3H',2,1,0))
for align in (0,4):
 call('_glVertexPointer',[2,0x140c,10,V],align);call('_glEnableClientState',[0x8074],align)
 call('_glColorPointer',[4,0x140c,0,C],align);call('_glEnableClientState',[0x8076],align)
 call('_glNormalPointer',[0x140c,0,N],align);call('_glEnableClientState',[0x8075],align)
 call('_glClientActiveTexture',[0x84c0],align);call('_glTexCoordPointer',[2,0x140c,0,T0],align);call('_glEnableClientState',[0x8078],align)
 call('_glClientActiveTexture',[0x84c1],align);call('_glTexCoordPointer',[2,0x140c,0,T1],align);call('_glEnableClientState',[0x8078],align)
 before=call('gles_get_draw_calls',[],align)
 call('_glDrawElements',[4,3,0x1401,IDX],align)
 name,mode,ix,attrs=array_draws[-1]
 assert name=='glDrawElements' and mode==4 and ix==[2,0,2]
 assert attrs[0]==[(3.,-1.5),(1.,-.5),(3.,-1.5)]
 assert attrs[1]==[(1.,.5,0.,1.)]*3 and attrs[2]==[(0.,0.,1.)]*3
 assert attrs[3]==[(0.,1.)]*3 and attrs[4]==[(.5,.25)]*3 and array_unit==1
 call('_glDrawElements',[5,3,0x1403,IDX+9],align);assert array_draws[-1][2]==[2,1,0]
 call('_glDrawArrays',[4,1,2],align);assert array_draws[-1][2]==[1,2] and array_draws[-1][3][0]==[(2.,-1.),(3.,-1.5)]
 assert call('gles_get_draw_calls',[],align)==before+3
 for pname,pointer_expected in ((0x8090,V),(0x8092,C),(0x8091,N),(0x8098,T1)):
  call('_glGetPointerv',[pname,OUT],align);assert pointer(OUT)==pointer_expected
 for pname,expected in ((0x807b,0x140c),(0x807c,10),(0x84e1,0x84c1)):
  call('_glGetIntegerv',[pname,OUT],align);assert pointer(OUT)==expected
 # Simulate presentation overwriting client pointers/flags without running GPU.
 array_state.clear();array_enabled.clear();array_unit=0
 call('gles_client_arrays_restore',[],align)
 assert set(array_state)=={0,1,2,3,4} and array_enabled=={0,1,2,3,4} and array_unit==1
 # No backend draw for negative counts, unsupported index width, NULL enabled
 # arrays, unsupported texture units or budgets; actual GLES error slot is used.
 before=len(array_draws)
 for fn,args,error in (
  ('_glDrawElements',[4,0xffffffff,0x1403,IDX],0x501),
  ('_glDrawElements',[4,3,0x1405,IDX],0x500),
  ('_glDrawElements',[4,3,0x1403,0],0x502),
  ('_glDrawElements',[4,3,0x1403,0xfffffffe],0x502),
  ('_glDrawArrays',[4,0,65537],0x505),
  ('_glClientActiveTexture',[0x84c2],0x500)):
  call(fn,args,align);assert call('_glGetError',[],align)==error
 assert len(array_draws)==before and array_unit==1
 call('_glNormalPointer',[0x140c,0,0],align);call('_glDrawArrays',[4,0,3],align)
 assert call('_glGetError',[],align)==0x502 and len(array_draws)==before
 call('_glDisableClientState',[0x8075],align);call('_glDrawArrays',[4,0,3],align)
 assert 2 not in array_draws[-1][3] and call('_glGetError',[],align)==0
 before=len(array_draws)
 call('_glVertexPointer',[2,0x140c,0x7fffffff,0xfffffff0],align)
 call('_glDrawArrays',[4,0,3],align);assert call('_glGetError',[],align)==0x502
 assert len(array_draws)==before
 call('_glVertexPointer',[2,0x140c,10,V],align)
print(f'PASS: {calls-array_start} production ARM client-array calls, fixed color/normal/two-texture data, byte strides, u8/unaligned-u16 indexed draws, original pointer/query state, restoration and error guards; VitaGL draw boundary captured, no GPU image.')

# OES init -> game state -> actual gles_present -> restored game state.
# SDK/OS/GPU entrypoints are a deterministic state model, not a renderer.
import copy
for table in ('gles_imports',):
 n=struct.unpack('<I',read(symbols[table+'_count'],4))[0]
 for i in range(n):
  name,t,k=struct.unpack('<III',read(symbols[table]+12*i,12));entries[cstring(name)]=(t,k)
for name in ('gles_init','gles_eagl_present'):
 entries['__'+name]=(symbols[name],0)
model_names=('l_info','l_error','glActiveTexture','glTexEnvi','glTexEnvfv','glGetIntegerv','glGetFloatv','glGetBooleanv','glIsEnabled',
 'glGetError','glEnable','glDisable','glBindTexture','glColorMask','glClearColor','glColor4f','glColor4x',
 'glMatrixMode','glPushMatrix','glPopMatrix','glLoadIdentity','glOrthof','glViewport','glClear',
 'glBindFramebuffer','glGenFramebuffers','glGenTextures','glTexImage2D','glTexParameteri',
 'glFramebufferTexture2D','glCheckFramebufferStatus','vglUseTripleBuffering','vglInitExtended','vglSwapBuffers',
 'glGetString','glHint','glReadPixels','glCompressedTexImage2D')
model_native={symbols[name]&~1:name for name in model_names}
for address in model_native:gles_capture.pop(address,None)
state={'active':0,'caps':set(),'texture_on':[False,False],'binding':[0,0],'env':[0x2100,0x2100],
 'env_color':[(0.,0.,0.,0.),(0.,0.,0.,0.)],'scale':[(1.,1.),(1.,1.)],
 'mask':(1,1,1,1),'clear':(0.,0.,0.,0.),'color':(1.,1.,1.,1.),'matrix_mode':0x1700,
 'viewport':(0,0,960,544),'fbo':0,'matrices':{},'stacks':{},'error':0}
identity=tuple(1. if i%5==0 else 0. for i in range(16))
for key in ((0x1700,0),(0x1701,0),(0x1702,0),(0x1702,1)):state['matrices'][key]=identity;state['stacks'][key]=[]
resource_next=1000;swaps=[];transfers=[];hints=[]
STRING=OBJECT+0xd000;u.mem_write(STRING,b'Backend fixture string\0')

def model_hook(uc,address,size,_):
 global resource_next
 if address not in model_native:return
 name=model_native[address]
 registers=[uc.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
 stack=struct.unpack('<4I',uc.mem_read(uc.reg_read(UC_ARM_REG_SP),16));args=registers+list(stack)
 fl=lambda i:struct.unpack('<f',struct.pack('<I',uc.reg_read(UC_ARM_REG_S0+i)))[0]
 out=0;active=state['active'];key=(state['matrix_mode'],active if state['matrix_mode']==0x1702 else 0)
 if name=='l_info':pass
 elif name=='l_error':raise AssertionError('Production GLES init logged an error')
 elif name=='glActiveTexture':state['active']=args[0]-0x84c0;assert state['active'] in (0,1)
 elif name in ('glEnable','glDisable'):
  if args[0]==0xde1:state['texture_on'][active]=name=='glEnable'
  elif name=='glEnable':state['caps'].add(args[0])
  else:state['caps'].discard(args[0])
 elif name=='glIsEnabled':out=int(state['texture_on'][active] if args[0]==0xde1 else args[0] in state['caps'])
 elif name=='glBindTexture':assert args[0]==0xde1;state['binding'][active]=args[1]
 elif name=='glTexEnvi':
  assert args[0]==0x2300
  if args[1]==0x2200:state['env'][active]=args[2]
  elif args[1] in (0x8573,0xd1c):
   value=list(state['scale'][active]);value[int(args[1]==0xd1c)]=float(args[2]);state['scale'][active]=tuple(value)
  else:state['error']=0x500
 elif name=='glTexEnvfv':
  assert args[0]==0x2300
  if args[1]==0x2201:state['env_color'][active]=struct.unpack('<4f',uc.mem_read(args[2],16))
  else:
   assert args[1] in (0x8573,0xd1c)
   value=list(state['scale'][active]);value[int(args[1]==0xd1c)]=struct.unpack('<f',uc.mem_read(args[2],4))[0];state['scale'][active]=tuple(value)
 elif name in ('glGetIntegerv','glGetFloatv','glGetBooleanv'):
  p=args[0]
  values={0x84e0:(0x84c0+active,),0x8069:(state['binding'][active],),0xba0:(state['matrix_mode'],),0xba2:state['viewport'],
          0xc22:state['clear'],0xc23:state['mask'],0xb00:state['color'],0x8ca7:(0,)}.get(p)
  assert values is not None,hex(p)
  fmt='f' if name=='glGetFloatv' else 'B' if name=='glGetBooleanv' else 'I'
  values=[float(v) if fmt=='f' else int(bool(v)) if fmt=='B' else int(v) for v in values]
  uc.mem_write(args[1],struct.pack('<'+fmt*len(values),*values))
 elif name=='glGetError':out=state['error'];state['error']=0
 elif name=='glColorMask':state['mask']=tuple(v&255 for v in args[:4])
 elif name=='glClearColor':state['clear']=tuple(fl(i) for i in range(4))
 elif name=='glColor4f':state['color']=tuple(fl(i) for i in range(4))
 elif name=='glColor4x':state['color']=tuple(max(0.,min(1.,struct.unpack('<i',struct.pack('<I',v))[0]/65536.)) for v in args[:4])
 elif name=='glMatrixMode':state['matrix_mode']=args[0]
 elif name=='glPushMatrix':state['stacks'][key].append(state['matrices'][key])
 elif name=='glPopMatrix':state['matrices'][key]=state['stacks'][key].pop()
 elif name=='glLoadIdentity':state['matrices'][key]=identity
 elif name=='glOrthof':state['matrices'][key]=tuple(fl(i) for i in range(6))
 elif name=='glViewport':state['viewport']=tuple(args[:4])
 elif name=='glBindFramebuffer':assert args[0]==0x8d40;state['fbo']=args[1]
 elif name in ('glGenFramebuffers','glGenTextures'):
  for i in range(args[0]):put(args[1]+4*i,resource_next);resource_next+=1
 elif name=='glCheckFramebufferStatus':out=0x8cd5
 elif name=='vglInitExtended':out=1
 elif name=='vglSwapBuffers':swaps.append((copy.deepcopy(state),copy.deepcopy(array_state),set(array_enabled),array_unit))
 elif name=='glGetString':out=STRING
 elif name=='glHint':hints.append(tuple(args[:2]))
 elif name=='glReadPixels':transfers.append((name,tuple(args[:7])));uc.mem_write(args[6],b'\x12\x34\x56\x78')
 elif name=='glCompressedTexImage2D':transfers.append((name,tuple(args[:8]),bytes(uc.mem_read(args[7],args[6]))))
 else:assert name in ('glClear','glTexImage2D','glTexParameteri','glFramebufferTexture2D','vglUseTripleBuffering')
 uc.reg_write(UC_ARM_REG_R0,out);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
u.hook_add(UC_HOOK_CODE,model_hook)
present_start=calls
call('gles_init',[])
FB=OBJECT+0xa000;RB=FB+4;OUT=RB+32;DATA=OUT+128
call('_glGenFramebuffersOES',[1,FB]);call('_glGenRenderbuffersOES',[1,RB]);fb=pointer(FB);rb=pointer(RB)
call('_glBindFramebufferOES',[0x8d40,fb]);call('_glBindRenderbufferOES',[0x8d41,rb])
call('_glRenderbufferStorageOES',[0x8d41,0x8058,320,480])
call('_glFramebufferRenderbufferOES',[0x8d40,0x8ce0,0x8d41,rb])
assert call('_glCheckFramebufferStatusOES',[0x8d40])==0x8cd5
assert call('_glGetError',[])==0
for align in (0,4):
 # All enum/floating/fixed routes, including vector enums unsupported by a
 # direct SDK glTexEnvfv call, must update the intended unit only.
 call('_glActiveTexture',[0x84c0],align)
 call('_glTexEnvf',[0x2300,0x2200,float_word(0x2101)],align)
 assert state['env'][0]==0x2101
 call('_glTexEnvf',[0x2300,0x8573,float_word(2.)],align)
 call('_glTexEnvx',[0x2300,0xd1c,4*65536],align);assert state['scale'][0]==(2.,4.)
 put(DATA,float_word(-1.),float_word(.25),float_word(2.),float_word(1.))
 call('_glTexEnvfv',[0x2300,0x2201,DATA],align);assert state['env_color'][0]==(0.,.25,1.,1.)
 put(DATA,0xffff0000,32768,2*65536,65536)
 call('_glTexEnvxv',[0x2300,0x2201,DATA],align);assert state['env_color'][0]==(0.,.5,1.,1.)
 put(DATA,float_word(0x2101));call('_glTexEnvfv',[0x2300,0x2200,DATA],align);assert state['env'][0]==0x2101
 call('_glActiveTexture',[0x84c1],align);call('_glTexEnvx',[0x2300,0x2200,0x1e01],align)
 assert state['env']==[0x2101,0x1e01]
 for name,args,error in (
  ('_glActiveTexture',[0x84c2],0x500),('_glTexEnvf',[0x2300,0x2200,float_word(math.nan)],0x500),
  ('_glTexEnvf',[0x2300,0x8573,float_word(2.5)],0x501),('_glTexEnvx',[0x2300,0xd1c,2],0x501),
  ('_glTexEnvfv',[0x2300,0x2200,0],0x501),('_glTexEnvxv',[0x2300,0x2201,0],0x501),
  ('_glTexEnvi',[0x2300,0x2200,0xdead],0x500),('_glTexEnvf',[0,0x2200,float_word(0x2100)],0x500)):
  call(name,args,align);assert call('_glGetError',[],align)==error
 assert state['active']==1 and state['env']==[0x2101,0x1e01]
 # Cached client/FBO queries must not leak temporary native float pointer types
 # or actual GPU IDs, and booleans must write exactly one byte.
 put(OUT,0xa5a5a5a5);call('_glGetBooleanv',[0x8074,OUT],align);assert get(OUT,1)==(0xa5a5a501,)
 call('_glGetIntegerv',[0x84e2,OUT],align);assert pointer(OUT)==2
 call('_glGetFloatv',[0x807b,OUT],align);assert struct.unpack('<f',u.mem_read(OUT,4))[0]==float(0x140c)
 call('_glGetFloatv',[0x8ca6,OUT],align);assert struct.unpack('<f',u.mem_read(OUT,4))[0]==float(fb)
 for name in ('_glGetFloatv','_glGetBooleanv'):
  call(name,[0x8074,0],align);assert call('_glGetError',[],align)==0x501
 assert call('_glGetString',[0x1f03],align)==STRING
 call('_glHint',[0xc54,0x1102],align);assert hints[-1]==(0xc54,0x1102)
 put(OUT,0,0xa5a5a5a5)
 call('_glReadPixels',[3,4,1,1,0x1908,0x1401,OUT],align)
 assert transfers[-1][1]==(3,4,1,1,0x1908,0x1401,OUT) and bytes(u.mem_read(OUT,8))==b'\x12\x34\x56\x78\xa5\xa5\xa5\xa5'
 u.mem_write(DATA,bytes(range(32)))
 call('_glCompressedTexImage2D',[0xde1,0,0x8c02,8,8,0,32,DATA],align)
 assert transfers[-1][1]==(0xde1,0,0x8c02,8,8,0,32,DATA) and transfers[-1][2]==bytes(range(32))
 # A non-default game state including both textures, fog/lighting, masks and
 # texture matrices must survive the actual presentation function unchanged.
 for cap in (0xbe2,0xbc0,0xc11,0xb71,0xb60,0xb50,0xb44,0xb90):call('_glEnable',[cap],align)
 for unit,texture in ((0x84c0,77),(0x84c1,88)):
  call('_glActiveTexture',[unit],align);call('_glBindTexture',[0xde1,texture],align);call('_glEnable',[0xde1],align)
 call('_glColorMask',[1,0,1,0],align);call('_glClearColor',[float_word(v) for v in (.125,.25,.375,.5)],align)
 call('_glColor4x',[16384,32768,49152,65536],align);call('_glViewport',[10,20,300,400],align)
 for k in state['matrices']:state['matrices'][k]=tuple(float(i+10+k[1]) for i in range(16))
 call('_glMatrixMode',[0x1702],align)
 call('_glDrawArrays',[4,0,3],align)
 before=copy.deepcopy(state);before_arrays=copy.deepcopy(array_state);before_enabled=set(array_enabled);before_unit=array_unit
 assert call('gles_eagl_present',[0x8d41],align)==1
 assert state==before,(state,before)
 assert array_state==before_arrays and array_enabled==before_enabled and array_unit==before_unit
 picture,ap,enabled,client=swaps[-1]
 assert picture['fbo']==0 and picture['viewport']==(0,0,960,544) and not picture['caps']
 assert picture['texture_on']==[True,False] and picture['mask']==(1,1,1,1) and picture['clear']==(0.,0.,0.,1.)
 assert picture['env'][0]==0x1e01 and picture['matrices'][(0x1702,0)]==identity
 assert enabled=={0,3} and client==0
 assert array_draws[-1][0]=='glDrawArrays' and array_draws[-1][2]==[0,1,2,3]
 assert array_draws[-1][3][0]==[(72.,0.),(888.,0.),(72.,544.),(888.,544.)]
 assert array_draws[-1][3][3]==[(1.,1.),(1.,0.),(0.,1.),(0.,0.)]
 assert call('_glGetError',[],align)==0
print(f'PASS: {calls-present_start} production texture/query/transfer/present calls; actual OES resource path and state save/restore with two units, fog/light/masks/matrices and 960x544 quad. SDK/GPU entrypoints modeled; no real pixels or owner game execution.')

text_count=struct.unpack('<I',read(symbols['libc_text_imports_count'],4))[0]
assert text_count==10
for i in range(text_count):
 n,t,k=struct.unpack('<III',read(symbols['libc_text_imports']+12*i,12));entries[cstring(n)]=(t,k)
text_start=calls
A=OBJECT+0xe000;B=A+256;W=A+512;X=W+128;OUT=X+128
u.mem_write(A,b'Iron Man 2\0');u.mem_write(B,b'iRON mAN 2\0')
for align in (0,4):
 u.mem_write(B,b'iRON mAN 2\0')
 assert call('_strcasecmp',[A,B],align)==0
 assert call('_strncasecmp',[A,B,4],align)==0
 u.mem_write(B,b'iRON Else\0');assert call('_strncasecmp',[A,B,4],align)==0
 assert call('_strcasecmp',[A,B],align)!=0
 u.mem_write(B,b'iRON mAN 2\0')
 for size in (0,1,5,11,16):
  u.mem_write(OUT,b'\xa5'*24)
  assert call('_strlcpy',[OUT,A,size],align)==10
  expected=(b'Iron Man 2'[:size-1]+b'\0') if size else b''
  assert bytes(u.mem_read(OUT,len(expected)))==expected
  assert bytes(u.mem_read(OUT+len(expected),24-len(expected)))==b'\xa5'*(24-len(expected))
 assert call('_strcoll',[A,A],align)==0
 put(W,ord('I'),ord('r'),ord('o'),ord('n'),0x1f600,0)
 assert call('_wcslen',[W],align)==5
 put(X,*([0xa5a5a5a5]*8));assert call('_wcscpy',[X,W],align)==X
 assert get(X,6)==get(W,6) and get(X+24,2)==(0xa5a5a5a5,0xa5a5a5a5)
 assert call('_wcscmp',[W,X],align)==0
 put(X,ord('Z'),0);assert call('_wcscmp',[W,X],align)&0x80000000
 ascii_wide=[ord(c) for c in 'Iron Man 2']+[0];put(W,*ascii_wide)
 assert call('_wcstombs',[0,W,0],align)==10
 u.mem_write(OUT,b'\xa5'*24)
 assert call('_wcstombs',[OUT,W,5],align)==5 and bytes(u.mem_read(OUT,6))==b'Iron \xa5'
 assert call('_wcstombs',[OUT,W,20],align)==10 and bytes(u.mem_read(OUT,11))==b'Iron Man 2\0'
 for n in (0,12345,-12345,2147483647,-2147483648):
  u.mem_write(B,str(n).encode()+b'\0');assert call('_atol',[B],align)==(n&0xffffffff)
 for n in (0,1,2**32+1,2**53+1,2**63,2**64-1):
  u.reg_write(UC_ARM_REG_D0,0x7ff8000000000000)
  lo=call('___floatundidf',[n&0xffffffff,n>>32],align)
  actual=struct.unpack('<d',struct.pack('<II',lo,u.reg_read(UC_ARM_REG_R1)))[0]
  assert actual==float(n),(n,actual)
print(f'PASS: {calls-text_start} real ARM SDK text/helper calls, truncation guards, case/count semantics, 32-bit wide Unicode copy/length, C-locale ASCII conversion, signed32 atol and unsigned64 SoftFP double returns. Non-C locales and Darwin errno mapping remain unverified.')
