/* SPDX-License-Identifier: MIT. Asset-free fixtures; optional private owner image argument. */
#include "cfstring.h"
#include "collections.h"
#include "foundation.h"
#include "darwin_printf.h"
#include <assert.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char fixture[512];
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t x){for(int i=0;i<4;i++)p[i]=(unsigned char)(x>>(i*8));}
static const uint8_t *read_fixture(void *ctx,uint32_t address,size_t bytes,int code){(void)ctx;(void)code;if(address<0x1000 || (uint64_t)address+bytes>0x1000+sizeof(fixture))return NULL;return fixture+address-0x1000;}
static Im2Imp method(void *o,const char *sel){Im2ObjcTarget t;assert(!im2_objc_lookup_target(o,sel,NULL,&t));assert(!t.guest);return t.imp;}
static uintptr_t send(void *o,const char *sel){return ((uintptr_t(*)(void *,const char *))method(o,sel))(o,sel);}
static void row(size_t n,uint32_t cls,uint32_t flags,uint32_t data,uint32_t length){unsigned char *p=fixture+16*n;put(p,cls);put(p+4,flags);put(p+8,data);put(p+12,length);}
static void synthetic(Im2ObjcClass *strings){
    assert((uintptr_t)strings<=UINT32_MAX);char error[160],out[64];
    row(0,(uint32_t)(uintptr_t)strings,0x7c8,0x1100,3);memcpy(fixture+256,"ABC",4);
    row(1,(uint32_t)(uintptr_t)strings,0x7d0,0x1120,4);
    const unsigned char utf16[]={0x41,0,0x5f,1,0x3d,0xd8,0x42,0xde,0,0};memcpy(fixture+288,utf16,sizeof(utf16));
    row(2,(uint32_t)(uintptr_t)strings,0x7c8,0x1140,3);memcpy(fixture+320,"A\0B",4);
    unsigned char unchanged[sizeof(fixture)];memcpy(unchanged,fixture,sizeof(fixture));
    assert(!im2_foundation_register_cfstrings(0x1000,3,read_fixture,NULL,error,sizeof(error)));
    assert(!memcmp(unchanged,fixture,sizeof(fixture)));
    void *a=(void *)0x1000,*b=(void *)0x1010,*c=(void *)0x1020;
    assert(im2_objc_object_class(a)==strings && send(a,"length")==3 && send(b,"length")==4);
    assert(!strcmp((char *)send(a,"UTF8String"),"ABC"));
    assert(!strcmp(im2_string_utf8(b),"A\xc5\x9f\xf0\x9f\x99\x82"));
    assert(!im2_string_get_cstring(c,out,sizeof(out),IM2_NS_UTF8) && !memcmp(out,"A\0B",4));
    assert(im2_string_get_cstring(b,out,sizeof(out),IM2_NS_ASCII));
    assert(im2_string_get_cstring(b,out,7,IM2_NS_UTF8));
    assert(im2_objc_retain(a)==a && !im2_objc_release(a) && im2_objc_autorelease(a)==a);
    assert(send(a,"copy")== (uintptr_t)a && im2_objc_retain_count(a)==SIZE_MAX);
    darwin_set_object_string_resolver(im2_string_utf8);
    uint32_t args[]={0x1000,0x1010,42};words w={NULL,args};
    assert(darwin_vformat(out,sizeof(out),"%@ %@ %d",&w)==14);
    assert(!strcmp(out,"ABC A\xc5\x9f\xf0\x9f\x99\x82 42"));
    uint32_t unknown[]={0xdeadbeef};w=(words){NULL,unknown};assert(darwin_vformat(out,sizeof(out),"%@",&w)==-1);
    darwin_set_object_string_resolver(NULL);w=(words){NULL,args};assert(darwin_vformat(out,sizeof(out),"%@",&w)==-1);
    /* A trailing length modifier must consume no argument (NULL ap/p would fault). */
    w=(words){NULL,NULL};assert(darwin_vformat(out,sizeof(out),"%ll",&w)==0);
    assert(im2_foundation_register_cfstrings(0x1000,3,read_fixture,NULL,error,sizeof(error)));
    assert(im2_objc_retain_count(a)==SIZE_MAX);assert(!im2_cfstrings_clear());
    assert(!im2_objc_object_class(a) && !im2_string_utf8(a));
    /* Reject the entire batch even when the first row is valid. */
    put(fixture+20,0x7d1);assert(im2_foundation_register_cfstrings(0x1000,2,read_fixture,NULL,error,sizeof(error)));
    assert(!im2_objc_object_class(a));put(fixture+20,0x7d0);
    fixture[292]=0;fixture[293]=0xdc;assert(im2_foundation_register_cfstrings(0x1000,2,read_fixture,NULL,error,sizeof(error)));
    memcpy(fixture,unchanged,sizeof(fixture));put(fixture+16,1);assert(im2_foundation_register_cfstrings(0x1000,2,read_fixture,NULL,error,sizeof(error)));
    memcpy(fixture,unchanged,sizeof(fixture));put(fixture+24,0x1121);assert(im2_foundation_register_cfstrings(0x1000,2,read_fixture,NULL,error,sizeof(error)));
    memcpy(fixture,unchanged,sizeof(fixture));put(fixture+24,0x11fe);assert(im2_foundation_register_cfstrings(0x1000,2,read_fixture,NULL,error,sizeof(error)));
    memcpy(fixture,unchanged,sizeof(fixture));fixture[259]=1;assert(im2_foundation_register_cfstrings(0x1000,1,read_fixture,NULL,error,sizeof(error)));
    memcpy(fixture,unchanged,sizeof(fixture));fixture[256]=0x80;assert(im2_foundation_register_cfstrings(0x1000,1,read_fixture,NULL,error,sizeof(error)));
    memcpy(fixture,unchanged,sizeof(fixture));assert(im2_foundation_register_cfstrings(0x1001,1,read_fixture,NULL,error,sizeof(error)));
    assert(im2_foundation_register_cfstrings(0xfffffff0,2,read_fixture,NULL,error,sizeof(error)));
    assert(im2_foundation_register_cfstrings(0x1000,4097,read_fixture,NULL,error,sizeof(error)));
    assert(!im2_objc_object_class(a));assert(!im2_cfstrings_clear());
    puts("PASS: ASCII/UTF-16 literals, embedded NUL, actual NSString methods, immortal ownership, immutable bytes, transactional rejection and safe %@ resolution");
}
typedef struct {uint32_t address,size,offset;} Segment;
typedef struct {unsigned char *file;size_t size;Segment segments[32];size_t count;} Image;
static const uint8_t *read_image(void *ctx,uint32_t address,size_t bytes,int code){
    (void)code;Image *im=ctx;
    for(size_t i=0;i<im->count;i++){Segment *s=im->segments+i;
        if(address>=s->address && (uint64_t)address+bytes<=(uint64_t)s->address+s->size)
            return im->file+s->offset+(address-s->address);
    }return NULL;
}
static void owner(const char *path,Im2ObjcClass *strings){
    FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>=28 && n<64*1024*1024);rewind(f);
    Image im={0};im.size=(size_t)n;im.file=malloc(im.size);assert(im.file);assert(fread(im.file,1,im.size,f)==im.size);assert(!fclose(f));
    assert(u32(im.file)==0xfeedface && u32(im.file+4)==12);
    size_t pos=28;uint32_t cfaddr=0,cfsize=0;
    for(uint32_t i=0;i<u32(im.file+16);i++){
        assert(pos+8<=im.size);uint32_t cmd=u32(im.file+pos),len=u32(im.file+pos+4);assert(len>=8 && len<=im.size-pos);
        if(cmd==1){assert(len>=56 && im.count<32);uint32_t off=u32(im.file+pos+32),bytes=u32(im.file+pos+36);assert((uint64_t)off+bytes<=im.size);
            im.segments[im.count++]=(Segment){u32(im.file+pos+24),bytes,off};
            uint32_t sections=u32(im.file+pos+48);assert((uint64_t)sections*68+56<=len);
            for(uint32_t j=0;j<sections;j++){unsigned char *s=im.file+pos+56+j*68;
                if(!memcmp(s,"__cfstring\0",11)){assert(!cfaddr);cfaddr=u32(s+32);cfsize=u32(s+36);}}
        }pos+=len;
    }
    assert(cfaddr && cfsize && cfsize%16==0);size_t count=cfsize/16;
    unsigned char *rows=(unsigned char *)read_image(&im,cfaddr,cfsize,0);assert(rows);
    /* Model only the resolved external ISA write; no ARM instruction is executed. */
    for(size_t i=0;i<count;i++)put(rows+16*i,(uint32_t)(uintptr_t)strings);
    unsigned char *snapshot=malloc(cfsize);assert(snapshot);memcpy(snapshot,rows,cfsize);
    char error[160];assert(!im2_foundation_register_cfstrings(cfaddr,count,read_image,&im,error,sizeof(error)));
    void **identities=calloc(count,sizeof(*identities));assert(identities);
    for(size_t i=0;i<count;i++)identities[i]=(void *)(uintptr_t)(cfaddr+i*16);
    void *array=im2_array_create(identities,count,0,0);assert(array && send(array,"count")==count);
    void *dict=im2_dictionary_create(identities,identities,count,0,0);assert(dict);
    size_t ascii=0,wide=0;
    for(size_t i=0;i<count;i++){
        const unsigned char *r=rows+16*i;uint32_t flags=u32(r+4),units=u32(r+12);void *id=(void *)(uintptr_t)(cfaddr+i*16);
        size_t bytes=0;uint32_t decoded_units=0;const char *decoded=im2_cfstring_utf8(id,&bytes,&decoded_units);assert(decoded && decoded_units==units);
        assert(send(id,"length")==units && (const char *)send(id,"UTF8String")==decoded);
        const char *raw=(const char *)read_image(&im,u32(r+8),(size_t)units*(flags==0x7d0?2:1),0);assert(raw);
        if(flags==0x7c8){ascii++;assert(bytes==units && !memcmp(decoded,raw,bytes));}
        else {
            wide++;iconv_t cd=iconv_open("UTF-8","UTF-16LE");assert(cd!=(iconv_t)-1);
            size_t inleft=(size_t)units*2,capacity=(size_t)units*4+1,outleft=capacity;char *expected=malloc(capacity),*write=expected,*read=(char *)raw;assert(expected);
            assert(iconv(cd,&read,&inleft,&write,&outleft)!=(size_t)-1 && !inleft);
            assert(capacity-outleft==bytes && !memcmp(expected,decoded,bytes));free(expected);assert(!iconv_close(cd));
        }
        assert(im2_objc_retain(id)==id && !im2_objc_release(id) && im2_objc_autorelease(id)==id && send(id,"copy")== (uintptr_t)id);
    }
    for(size_t i=0;i<count;i++) {
        const char *sel="objectForKey:";
        void *value=(void *)((uintptr_t(*)(void *,const char *,void *))method(dict,sel))(dict,sel,identities[i]);
        assert(value && im2_string_byte_length(value)==im2_string_byte_length(identities[i]));
        assert(!memcmp(im2_string_utf8(value),im2_string_utf8(identities[i]),im2_string_byte_length(value)));
    }
    assert(!im2_objc_release(dict) && !im2_objc_release(array));free(identities);
    assert(!memcmp(snapshot,rows,cfsize));assert(!im2_cfstrings_clear());
    printf("PASS private owner metadata only: %zu literals, %zu ASCII, %zu UTF-16 independently decoded with iconv; no guest ARM execution\n",count,ascii,wide);
    free(snapshot);free(im.file);
}
int main(int argc,char **argv){
    Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);assert(root);
    Im2FoundationConfig cfg={"/tmp",NULL,NULL,NULL};assert(!im2_foundation_init(root,&cfg));
    Im2ObjcClass *strings=im2_objc_class_named("NSString");assert(strings);
    assert(!im2_collections_init(root,NULL));
    synthetic(strings);if(argc==2)owner(argv[1],strings);else assert(argc==1);
}
