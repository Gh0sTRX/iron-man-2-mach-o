#!/usr/bin/env python3
"""Inspect ABI flags and target addresses in the actual linked ARM ELF import table."""
import argparse
from pathlib import Path
import struct

p=argparse.ArgumentParser();p.add_argument('--elf',required=True);a=p.parse_args()
b=Path(a.elf).read_bytes()
assert b[:7]==b'\x7fELF\x01\x01\x01'
h=struct.unpack_from('<HHIIIIIHHHHHH',b,16)
assert h[1]==40  # EM_ARM
shoff,shsize,shcount=h[5],h[10],h[11]
sections=[struct.unpack_from('<10I',b,shoff+shsize*i) for i in range(shcount)]
def read(va,n):
    for s in sections:
        if s[1]!=8 and s[3]<=va and va+n<=s[3]+s[5]:
            off=s[4]+va-s[3]
            return b[off:off+n]
    raise AssertionError(f'unmapped ELF address {va:x}')
def cstring(va):
    text=bytearray()
    for i in range(256):
        c=read(va+i,1)
        if c==b'\0':return text.decode()
        text.extend(c)
    raise AssertionError('unterminated ELF string')
symbols={}
for s in sections:
    if s[1]!=2:continue
    strings=sections[s[6]]
    for off in range(s[4],s[4]+s[5],s[9]):
        name,value,size,info,other,index=struct.unpack_from('<IIIBBH',b,off)
        end=b.index(0,strings[4]+name,strings[4]+strings[5])
        symbols[b[strings[4]+name:end].decode()]=value
count=struct.unpack('<I',read(symbols['objc_imports_count'],4))[0]
entries={}
for i in range(count):
    name,target,kind=struct.unpack('<3I',read(symbols['objc_imports']+12*i,12))
    entries[cstring(name)]=(target,kind)
for name in ('objc_msgSend','objc_msgSend_stret','objc_msgSendSuper2','objc_msgSendSuper2_stret'):
    target,kind=entries['_'+name]
    assert kind==2, (name,'assembly dispatcher must bypass generic stack-copy thunk')
    assert target==symbols['im2_'+name+'_entry']
for name in ('objc_getClass','CFStringGetCString','NSClassFromString','objc_enumerationMutation'):
    target,kind=entries['_'+name]
    assert target and kind==0, (name,'C bridges must use the ABI thunk')
assert '_UIApplicationMain' in entries
entry,kind=entries['_UIApplicationMain']
assert entry and kind==0, 'Legacy UIApplicationMain must bind to the nonempty native C launch stage'
eagl_count=struct.unpack('<I',read(symbols['eagl_imports_count'],4))[0]
assert eagl_count==3
eagl={}
for i in range(eagl_count):
    name,target,kind=struct.unpack('<3I',read(symbols['eagl_imports']+12*i,12))
    eagl[cstring(name)]=(target,kind)
assert set(eagl)=={
    '_kEAGLDrawablePropertyRetainedBacking',
    '_kEAGLDrawablePropertyColorFormat',
    '_kEAGLColorFormatRGB565'
}
assert all(target and kind==1 for target,kind in eagl.values()), 'EAGL NSString globals must be bound as data, never function thunks'
print('PASS: linked ARM ELF includes ObjC message dispatch, UIApplicationMain and three EAGL NSString data constants')

# Verify the actual linked OES import entries reach the stateful wrappers.
gles_count=struct.unpack('<I',read(symbols['gles_imports_count'],4))[0]
gles_entries={}
for i in range(gles_count):
    name,target,kind=struct.unpack('<3I',read(symbols['gles_imports']+12*i,12))
    gles_entries[cstring(name)]=(target,kind)
for name in ('glGenFramebuffersOES','glGenRenderbuffersOES','glBindFramebufferOES',
             'glBindRenderbufferOES','glDeleteFramebuffersOES','glDeleteRenderbuffersOES',
             'glRenderbufferStorageOES','glFramebufferRenderbufferOES',
             'glCheckFramebufferStatusOES','glGetRenderbufferParameterivOES',
             'glGetIntegerv','glGetError'):
    assert gles_entries['_'+name]==(symbols['g_'+name],0),name
print('PASS: linked OES imports reach actual resource-mapping wrappers, not no-op callbacks')

cf_count=struct.unpack('<I',read(symbols['cfbundle_imports_count'],4))[0]
assert cf_count==5
cf={}
for i in range(cf_count):
    name,target,kind=struct.unpack('<3I',read(symbols['cfbundle_imports']+12*i,12))
    cf[cstring(name)]=(target,kind)
assert set(cf)=={'_CFBundleGetMainBundle','_CFBundleCopyBundleURL',
                 '_CFBundleCopyResourcesDirectoryURL','_CFURLCopyFileSystemPath',
                 '_CFURLGetFileSystemRepresentation'}
assert all(target and kind==0 for target,kind in cf.values())
assert cf['_CFBundleCopyBundleURL']==cf['_CFBundleCopyResourcesDirectoryURL']
print('PASS: linked CFBundle/CFURL imports are real function entries with flat iOS resource-root alias')

legacy_count=struct.unpack('<I',read(symbols['legacy_string_imports_count'],4))[0]
assert legacy_count==24
legacy={}
for i in range(legacy_count):
    name,target,kind=struct.unpack('<3I',read(symbols['legacy_string_imports']+12*i,12))
    legacy[cstring(name)]=(target,kind)
empty='__ZNSs4_Rep20_S_empty_rep_storageE'
assert legacy[empty]==(symbols['_ZNSs4_Rep20_S_empty_rep_storageE'],1)
assert all(target and kind==0 for name,(target,kind) in legacy.items() if name!=empty)
assert '__ZNKSs6substrEmm' in legacy and '__ZNSs6insertEmRKSsmm' in legacy
print('PASS: 23 ARM C++ function bindings and actual SDK empty-rep DATA symbol')

math_count=struct.unpack('<I',read(symbols['math_bridge_imports_count'],4))[0]
assert math_count==23
for i in range(math_count):
    name,target,kind=struct.unpack('<3I',read(symbols['math_bridge_imports']+12*i,12))
    assert kind==0 and target==symbols['b_'+cstring(name)[1:]], cstring(name)
print('PASS: 23 ARM math imports route to explicit base-AAPCS bridges')

container_count=struct.unpack('<I',read(symbols['legacy_container_imports_count'],4))[0]
assert container_count==8
container_targets={}
for i in range(container_count):
    name,target,kind=struct.unpack('<3I',read(symbols['legacy_container_imports']+12*i,12))
    assert target and kind==0
    container_targets[cstring(name)]=target
assert len(container_targets)==8 and len(set(container_targets.values()))==8
assert '__ZNSt15_List_node_base8transferEPS_S0_' in container_targets
assert '__ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_' in container_targets
print('PASS: eight legacy C++ node imports are linked function bridges')

cg_count=struct.unpack('<I',read(symbols['cg_geometry_imports_count'],4))[0]
assert cg_count==5
cg_entries={}
for i in range(cg_count):
    name,target,kind=struct.unpack('<3I',read(symbols['cg_geometry_imports']+12*i,12))
    cg_entries[cstring(name)]=(target,kind)
for name,size in (('_CGPointZero',8),('_CGRectZero',16)):
    target,kind=cg_entries[name];assert kind==1 and read(target,size)==bytes(size)
for name,target in (('_CGAffineTransformMakeRotation','cg_rotation'),
                    ('_CGAffineTransformMakeScale','cg_scale'),('_CGRectOffset','cg_rect_offset')):
    assert cg_entries[name]==(symbols[target],0)
print('PASS: CoreGraphics zero globals are real data; three aggregate functions use geometry bridges')

cf_memory_count=struct.unpack('<I',read(symbols['cf_memory_imports_count'],4))[0]
assert cf_memory_count==2
cf_memory={}
for i in range(cf_memory_count):
    name,target,kind=struct.unpack('<3I',read(symbols['cf_memory_imports']+12*i,12))
    cf_memory[cstring(name)]=(target,kind)
assert cf_memory=={'_CFRelease':(symbols['cf_release'],0),'_CFRetain':(symbols['cf_retain'],0)}
cf_array_count=struct.unpack('<I',read(symbols['cfarray_imports_count'],4))[0]
assert cf_array_count==6
cf_array={}
for i in range(cf_array_count):
    name,target,kind=struct.unpack('<3I',read(symbols['cfarray_imports']+12*i,12))
    cf_array[cstring(name)]=(target,kind)
assert set(cf_array)=={'_CFArrayCreateMutable','_CFArrayAppendValue','_CFArrayGetCount',
                      '_CFArrayGetValueAtIndex','_CFArrayRemoveAllValues','_kCFAllocatorDefault'}
target,kind=cf_array['_kCFAllocatorDefault'];assert kind==1 and read(target,4)==bytes(4)
assert all(target and kind==0 for name,(target,kind) in cf_array.items() if name!='_kCFAllocatorDefault')
print('PASS: CF array function imports, real default-allocator data and shared managed CF lifetime imports')

gf_count=struct.unpack('<I',read(symbols['gles_float_imports_count'],4))[0]
assert gf_count==19
gf_entries={}
for i in range(gf_count):
    name,target,kind=struct.unpack('<3I',read(symbols['gles_float_imports']+12*i,12))
    gf_entries[cstring(name)]=(target,kind)
for name,bridge in {'glAlphaFunc':'gf_alpha','glClearColor':'gf_clear','glClearDepthf':'gf_depth',
                    'glDepthRangef':'gf_range','glFogf':'gf_fog','glFrustumf':'gf_frustum',
                    'glOrthof':'gf_ortho','glRotatef':'gf_rotate','glScalef':'gf_scale',
                    'glTranslatef':'gf_translate','glMaterialf':'gf_material',
                    'glLightf':'gf_light_scalar','glLightfv':'gf_light_vector'}.items():
    assert gf_entries['_'+name]==(symbols[bridge],0)
for name in ('glFogfv','glLightModelfv','glLoadMatrixf','glMaterialfv','glMultMatrixf','glClearColorx'):
    assert gf_entries['_'+name]==(symbols[name],0)
assert '_glGetLightxv' not in gles_entries,'Unsupported old zero-filled light query must not be advertised'
print('PASS: 19 GLES state imports use explicit scalar ABI bridges or pointer/integer-safe backend entries; light validation bridge and no fake light getter')

for name in ('glColorPointer','glNormalPointer','glDrawElements','glVertexPointer',
             'glTexCoordPointer','glClientActiveTexture','glEnableClientState',
             'glDisableClientState','glDrawArrays','glGetPointerv'):
    assert gles_entries['_'+name]==(symbols['g_'+name],0), name
assert 'im2_arrays_draw' in symbols and 'im2_arrays_elements' in symbols
print('PASS: guest client arrays and both draw forms route to checked conversion/state bridges')

for name in ('glActiveTexture','glTexEnvi','glTexEnvf','glTexEnvfv','glTexEnvx','glTexEnvxv','glGetFloatv','glGetBooleanv'):
    assert gles_entries['_'+name]==(symbols['g_'+name],0),name
for name in ('glGetString','glHint','glReadPixels','glCompressedTexImage2D'):
    assert gles_entries['_'+name]==(symbols[name],0),name
print('PASS: texture/query state bridges and integer/pointer-only upload/readback APIs linked to real backend entries')

text_count=struct.unpack('<I',read(symbols['libc_text_imports_count'],4))[0]
assert text_count==10
text_entries={}
for i in range(text_count):
    name,target,kind=struct.unpack('<3I',read(symbols['libc_text_imports']+12*i,12))
    text_entries[cstring(name)]=(target,kind)
for name in ('atol','strcasecmp','strncasecmp','strlcpy','strcoll','wcscmp','wcscpy','wcslen','wcstombs'):
    assert text_entries['_'+name]==(symbols[name],0)
assert text_entries['___floatundidf']==(symbols['text_u64_double'],0)
print('PASS: nine native C text functions and explicit unsigned64-to-double base-AAPCS helper linked')

stream_count=struct.unpack('<I',read(symbols['legacy_stream_imports_count'],4))[0]
assert stream_count==8
stream_entries={}
for i in range(stream_count):
    name,target,kind=struct.unpack('<3I',read(symbols['legacy_stream_imports']+12*i,12))
    assert target and kind==0;stream_entries[cstring(name)]=target
assert len(stream_entries)==8 and '__ZNSolsEi' in stream_entries
print('PASS: eight legacy C++ stream/Init bindings linked as explicit exception-contained bridges')

time_count=struct.unpack('<I',read(symbols['time_bridge_imports_count'],4))[0]
assert time_count==4
time_entries={}
for i in range(time_count):
    name,target,kind=struct.unpack('<3I',read(symbols['time_bridge_imports']+12*i,12))
    time_entries[cstring(name)]=(target,kind)
assert time_entries=={'_CFAbsoluteTimeGetCurrent':(symbols['cf_now'],0),'_time':(symbols['time_now'],0),
                      '_difftime':(symbols['time_difference'],0),'_gmtime':(symbols['utc'],0)}
print('PASS: CF/Unix time and Darwin UTC calendar imports route to typed ABI bridges')
