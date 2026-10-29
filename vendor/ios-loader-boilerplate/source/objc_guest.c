/* SPDX-License-Identifier: MIT. Independent implementation of the objc4 ARM32 layout. */
#include "objc_guest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GUEST_CLASSES 64
#define GUEST_METHODS 64
typedef struct {
    uint32_t identity,meta,super,meta_super,meta_isa,size,start;
    char *name;
    Im2MethodSpec *instance_methods,*class_methods;
    size_t ni,nc;
    int registered;
} GuestClass;
static uint32_t word(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static char *string_at(Im2GuestRead read,void *ctx,uint32_t address,size_t limit) {
    if(!address) return NULL;
    char *s=calloc(limit+1,1);if(!s) return NULL;
    for(size_t i=0;i<=limit;i++) {
        if((uint64_t)address+i>UINT32_MAX) break;
        const uint8_t *p=read(ctx,address+(uint32_t)i,1,0);if(!p) break;
        if(!*p) return s;
        if(i<limit)s[i]=(char)*p;
    }
    free(s);return NULL;
}
static int methods_at(Im2GuestRead read,void *ctx,uint32_t address,Im2MethodSpec **out,size_t *count) {
    *out=NULL;*count=0;if(!address)return 0;
    if(address%4)return -1;
    const uint8_t *header=read(ctx,address,8,0);if(!header)return -1;
    uint32_t stride=word(header)&~3u,n=word(header+4);
    if(stride!=12 || n>GUEST_METHODS || (uint64_t)address+8+12ull*n>UINT32_MAX)return -1;
    const uint8_t *list=read(ctx,address+8,12u*n,0);if(!list && n)return -1;
    Im2MethodSpec *spec=calloc(n?n:1,sizeof(*spec));if(!spec)return -1;
    *out=spec;*count=n;
    for(uint32_t i=0;i<n;i++) {
        const uint8_t *m=list+12*i;uint32_t imp=word(m+8),pc=imp&~1u;
        if(!imp || (!(imp&1) && pc%4) || !read(ctx,pc,imp&1?2:4,1))return -1;
        spec[i].selector=string_at(read,ctx,word(m),255);
        spec[i].types=string_at(read,ctx,word(m+4),1024);
        if(!spec[i].selector || !*spec[i].selector || !spec[i].types || !*spec[i].types)return -1;
        spec[i].imp=(Im2Imp)(uintptr_t)imp;
        /* Scalar/float/pointer guest->guest returns preserve Darwin register bits.
         * Aggregates/unknown encodings stay explicitly unsupported until their exact
         * return convention is established. Registration is not ABI completion. */
        const char *type=spec[i].types;while(*type && strchr("rnNoORV",*type))type++;
        spec[i].stret_size=*type && strchr("v@#:*^cCsSiIlLqQfdB",*type)?0:SIZE_MAX;
    }
    return 0;
}
static void free_methods(Im2MethodSpec *s,size_t n) {
    for(size_t i=0;i<n;i++){free((void *)s[i].selector);free((void *)s[i].types);}free(s);
}
static int parse_class(GuestClass *out,uint32_t address,Im2GuestRead read,void *ctx) {
    if(!address || address%4)return -1;
    const uint8_t *c=read(ctx,address,20,0);if(!c)return -1;
    out->identity=address;out->meta=word(c);out->super=word(c+4);
    uint32_t data=word(c+16);
    if(!out->meta || out->meta==address || out->meta%4 || !data || data%4)return -1;
    const uint8_t *ro=read(ctx,data,40,0),*meta=read(ctx,out->meta,20,0);
    if(!ro || !meta || (word(ro)&(1u|4u|0x80000000u)))return -1;
    out->meta_isa=word(meta);out->meta_super=word(meta+4);
    uint32_t meta_data=word(meta+16);if(!meta_data || meta_data%4)return -1;
    const uint8_t *mr=read(ctx,meta_data,40,0);if(!mr || !(word(mr)&1u) || (word(mr)&(4u|0x80000000u)))return -1;
    out->start=word(ro+4);out->size=word(ro+8);
    if(out->size<4 || out->size>16*1024*1024u || out->start>out->size)return -1;
    out->name=string_at(read,ctx,word(ro+16),255);
    char *meta_name=string_at(read,ctx,word(mr+16),255);
    int names_ok=out->name && *out->name && meta_name && !strcmp(out->name,meta_name);
    free(meta_name);if(!names_ok)return -1;
    if(methods_at(read,ctx,word(ro+20),&out->instance_methods,&out->ni) ||
       methods_at(read,ctx,word(mr+20),&out->class_methods,&out->nc))return -1;
    return 0;
}
int im2_objc_register_guest_list(uint32_t list,size_t count,Im2GuestRead read,void *ctx,
    char *error,size_t error_size) {
    const char *why=NULL;GuestClass *classes=NULL;size_t checkpoint=im2_objc_registration_checkpoint();
    if(!read || count>GUEST_CLASSES || list%4 || (uint64_t)list+4ull*count>UINT32_MAX) {why="invalid guest class list";goto bad;}
    const uint8_t *p=count?read(ctx,list,4*count,0):NULL;
    if(count && !p){why="guest class list outside mapped image";goto bad;}
    classes=calloc(count?count:1,sizeof(*classes));if(!classes){why="guest class staging allocation failed";goto bad;}
    for(size_t i=0;i<count;i++) {
        if(parse_class(&classes[i],word(p+4*i),read,ctx)){why="invalid or unsupported ObjC2 class/method metadata";goto bad;}
        if(im2_objc_class_named(classes[i].name) || im2_objc_is_class((void *)(uintptr_t)classes[i].identity) ||
           im2_objc_is_class((void *)(uintptr_t)classes[i].meta)){why="guest class conflicts with existing registry";goto bad;}
        for(size_t j=0;j<i;j++) if(!strcmp(classes[j].name,classes[i].name) ||
            classes[j].identity==classes[i].identity || classes[j].meta==classes[i].meta ||
            classes[j].identity==classes[i].meta || classes[j].meta==classes[i].identity) {
            why="duplicate guest class/metaclass identity or name";goto bad;
        }
    }
    size_t remaining=count;
    while(remaining) {
        size_t progress=0;
        for(size_t i=0;i<count;i++) {
            GuestClass *c=&classes[i];if(c->registered)continue;
            const Im2ObjcClass *parent=im2_objc_class_metadata((void *)(uintptr_t)c->super);
            if(!parent)continue;
            const Im2ObjcClass *parent_meta=im2_objc_class_metadata(parent->isa);
            if((parent->data->ro->flags&1) || !parent_meta ||
               c->meta_super!=(uintptr_t)parent->isa || c->meta_isa!=(uintptr_t)parent_meta->isa ||
               c->start<parent->data->ro->instance_size || c->size<parent->data->ro->instance_size) {
                why="guest superclass/metaclass or instance layout mismatch";goto bad;
            }
            if(!im2_objc_register_foreign_class(c->name,(void *)(uintptr_t)c->identity,
                (void *)(uintptr_t)c->meta,(Im2ObjcClass *)(uintptr_t)c->super,c->size,
                c->instance_methods,c->ni,c->class_methods,c->nc)) {
                why="guest class registration rejected";goto bad;
            }
            c->registered=1;remaining--;progress++;
        }
        if(!progress){why="unresolved/cyclic guest superclass; no placeholder created";goto bad;}
    }
    if(error && error_size)error[0]=0;
    for(size_t i=0;i<count;i++){free(classes[i].name);free_methods(classes[i].instance_methods,classes[i].ni);free_methods(classes[i].class_methods,classes[i].nc);}free(classes);
    return 0;
bad:
    (void)im2_objc_registration_rollback(checkpoint);
    if(classes)for(size_t i=0;i<count;i++){free(classes[i].name);free_methods(classes[i].instance_methods,classes[i].ni);free_methods(classes[i].class_methods,classes[i].nc);}
    free(classes);if(error && error_size)snprintf(error,error_size,"%s",why);return -1;
}
