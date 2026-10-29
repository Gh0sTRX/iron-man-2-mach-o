#include "objc_guest.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define BASE 0x10000u
static uint8_t image[0x2000],backup[0x2000];
static const uint8_t *read_guest(void *ctx,uint32_t addr,size_t n,int exec) {
    (void)ctx;
    if(addr<BASE || (uint64_t)addr+n>BASE+sizeof(image))return NULL;
    if(exec && !(addr>=BASE+0x1800 && (uint64_t)addr+n<=BASE+0x1900))return NULL;
    return image+addr-BASE;
}
static void put(uint32_t addr,uint32_t v){uint8_t *p=image+addr-BASE;p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static uint32_t string(uint32_t addr,const char *s){memcpy(image+addr-BASE,s,strlen(s)+1);return addr;}
static void root_imp(void){}
static void make_class(uint32_t cls,uint32_t meta,uint32_t ro,uint32_t mro,uint32_t parent,uint32_t parentmeta,uint32_t rootmeta,const char *name,uint32_t nameaddr) {
    put(cls,meta);put(cls+4,parent);put(cls+16,ro);
    put(meta,rootmeta);put(meta+4,parentmeta);put(meta+16,mro);
    put(ro+4,8);put(ro+8,32);put(ro+16,string(nameaddr,name));
    put(mro,1);put(mro+8,20);put(mro+16,nameaddr);
}
int main(void) {
    const Im2MethodSpec root_methods[]={{"inherited","v@:",root_imp,0}};
    Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),root_methods,1,NULL,0);
    assert(root && (uintptr_t)root<=UINT32_MAX && (uintptr_t)root->isa<=UINT32_MAX);
    uint32_t host=(uint32_t)(uintptr_t)root,meta=(uint32_t)(uintptr_t)root->isa;
    make_class(BASE+0x100,BASE+0x120,BASE+0x200,BASE+0x240,host,meta,meta,"GuestBase",BASE+0x800);
    make_class(BASE+0x140,BASE+0x160,BASE+0x280,BASE+0x2c0,BASE+0x100,BASE+0x120,meta,"GuestChild",BASE+0x820);
    put(BASE+0x280+4,32);put(BASE+0x280+8,48);
    put(BASE,BASE+0x140);put(BASE+4,BASE+0x100); /* Child first: topological registration. */
    put(BASE+0x200+20,BASE+0x400);put(BASE+0x400,12);put(BASE+0x404,1);
    put(BASE+0x408,string(BASE+0x900,"guestMethod:"));put(BASE+0x40c,string(BASE+0x920,"I12@0:4I8"));put(BASE+0x410,BASE+0x1801);
    put(BASE+0x2c0+20,BASE+0x440);put(BASE+0x440,12);put(BASE+0x444,1);
    put(BASE+0x448,string(BASE+0x940,"makeGuest"));put(BASE+0x44c,string(BASE+0x960,"@8@0:4"));put(BASE+0x450,BASE+0x1810);
    memcpy(backup,image,sizeof(image));size_t checkpoint=im2_objc_registration_checkpoint();char error[160];
    /* Missing parent after a valid class was registered must roll the whole batch back. */
    put(BASE+0x140+4,0xdead0000);
    assert(im2_objc_register_guest_list(BASE,2,read_guest,NULL,error,sizeof(error)));
    assert(im2_objc_registration_checkpoint()==checkpoint && !im2_objc_class_named("GuestBase"));
    memcpy(image,backup,sizeof(image));
    int rc=im2_objc_register_guest_list(BASE,2,read_guest,NULL,error,sizeof(error));
    if(rc)fprintf(stderr,"unexpected rejection: %s\n",error);
    assert(!rc);
    assert(!memcmp(image,backup,sizeof(image))); /* Original metadata stays unchanged. */
    Im2ObjcClass *child=im2_objc_class_named("GuestChild");assert((uintptr_t)child==BASE+0x140);
    void *o=im2_objc_alloc(child,NULL);assert(o && ((Im2ObjcObject *)o)->isa==child);
    Im2ObjcTarget target;
    assert(!im2_objc_lookup_target(o,"guestMethod:",NULL,&target) && target.guest && (uintptr_t)target.imp==BASE+0x1801);
    assert(!im2_objc_lookup_target(o,"inherited",NULL,&target) && !target.guest && target.imp==root_imp);
    assert(!im2_objc_lookup_target(child,"makeGuest",NULL,&target) && target.guest && (uintptr_t)target.imp==BASE+0x1810);
    assert(!im2_objc_lookup_target(o,"inherited",(void *)(uintptr_t)(BASE+0x100),&target));
    assert(im2_objc_lookup_target(o,"inherited",root->isa,&target)); /* unrelated metaclass */
    assert(im2_objc_registration_rollback(checkpoint)); /* live instance prevents removal */
    assert(!im2_objc_release(o));assert(!im2_objc_registration_rollback(checkpoint));
    assert(root->data->first_subclass==NULL);
    struct {uint32_t slot,value;} bad[]={
        {BASE+0x200+20,BASE+0x1ffc},{BASE+0x400,16},{BASE+0x404,65},
        {BASE+0x408,BASE+0x1fff},{BASE+0x410,BASE+0x1802},
        {BASE+0x410,BASE+0x800},{BASE+0x120+4,meta+4},
        {BASE+0x240,0},{BASE+0x200,4},{BASE+0x100+16,BASE+0x1ffc},
        {BASE+4,BASE+0x140},{BASE+0x280+8,2}};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        memcpy(image,backup,sizeof(image));image[sizeof(image)-1]='x';put(bad[i].slot,bad[i].value);
        assert(im2_objc_register_guest_list(BASE,2,read_guest,NULL,error,sizeof(error)));
        assert(im2_objc_registration_checkpoint()==checkpoint && !im2_objc_class_named("GuestBase"));
    }
    memcpy(image,backup,sizeof(image));
    assert(im2_objc_register_guest_list(BASE+1,2,read_guest,NULL,error,sizeof(error)));
    assert(im2_objc_register_guest_list(BASE,65,read_guest,NULL,error,sizeof(error)));
    assert(im2_objc_register_guest_list(0xfffffffcu,2,read_guest,NULL,error,sizeof(error)));
    puts("PASS: original-identity ObjC2 registration, guest/native inherited dispatch, hierarchy, rollback and malformed metadata");
}
