/* SPDX-License-Identifier: MIT. Independently implemented from the ARM32 literal layout. */
#include "cfstring.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CONSTANT_LIMIT 4096u
#define UNIT_LIMIT 1048576u
#define MEMORY_LIMIT (64u*1024u*1024u)
typedef struct { void *identity;char *utf8;size_t bytes,capacity;uint32_t units; } Constant;
static Constant *constants;static size_t constant_count,memory;
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t half(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8;}
static int decode(Constant *out,const uint8_t *payload,uint32_t flags,uint32_t length) {
    out->capacity=(size_t)length*(flags==0x7d0?3u:1u)+1;
    out->utf8=malloc(out->capacity);if(!out->utf8)return -1;
    if(flags==0x7c8) {
        for(uint32_t i=0;i<length;i++){if(payload[i]>=128)return -1;out->utf8[i]=(char)payload[i];}
        out->bytes=length;
    } else {
        size_t n=0;
        for(uint32_t i=0;i<length;i++) {
            uint32_t c=half(payload+2*i);
            if(c>=0xd800 && c<=0xdbff) {
                if(++i==length)return -1;
                uint32_t lo=half(payload+2*i);if(lo<0xdc00 || lo>0xdfff)return -1;
                c=0x10000+((c-0xd800)<<10)+(lo-0xdc00);
            } else if(c>=0xdc00 && c<=0xdfff)return -1;
            if(c<0x80)out->utf8[n++]=(char)c;
            else if(c<0x800){out->utf8[n++]=(char)(0xc0|(c>>6));out->utf8[n++]=(char)(0x80|(c&63));}
            else if(c<0x10000){out->utf8[n++]=(char)(0xe0|(c>>12));out->utf8[n++]=(char)(0x80|((c>>6)&63));out->utf8[n++]=(char)(0x80|(c&63));}
            else {out->utf8[n++]=(char)(0xf0|(c>>18));out->utf8[n++]=(char)(0x80|((c>>12)&63));out->utf8[n++]=(char)(0x80|((c>>6)&63));out->utf8[n++]=(char)(0x80|(c&63));}
        }
        out->bytes=n;
    }
    out->units=length;out->utf8[out->bytes]=0;return 0;
}
const char *im2_cfstring_utf8(const void *identity,size_t *bytes,uint32_t *units) {
    for(size_t i=0;i<constant_count;i++)if(constants[i].identity==identity) {
        if(bytes)*bytes=constants[i].bytes;
        if(units)*units=constants[i].units;
        return constants[i].utf8;
    }
    return NULL;
}
int im2_cfstrings_register(uint32_t address,size_t count,Im2ObjcClass *cls,
    Im2GuestRead read,void *ctx,char *error,size_t size) {
    const char *why="invalid constant string section";Constant *staged=NULL,*combined=NULL;void **ids=NULL;size_t added=0;
    if(!read || !im2_objc_is_class(cls) || (uintptr_t)cls>UINT32_MAX || address%4 ||
       count>CONSTANT_LIMIT-constant_count || (uint64_t)address+16ull*count>UINT32_MAX)goto bad;
    if(!count){if(error && size)error[0]=0;return 0;}
    const uint8_t *rows=read(ctx,address,16*count,0);if(!rows)goto bad;
    staged=calloc(count,sizeof(*staged));ids=calloc(count,sizeof(*ids));
    if(!staged || !ids){why="constant staging allocation failed";goto bad;}
    for(size_t i=0;i<count;i++) {
        const uint8_t *r=rows+16*i;uint32_t flags=word(r+4),data=word(r+8),units=word(r+12);
        if(word(r)!=(uintptr_t)cls || (flags!=0x7c8 && flags!=0x7d0) || units>UNIT_LIMIT || !data) {
            why="unsupported CFString ISA/flags/length";goto bad;
        }
        size_t width=flags==0x7d0?2u:1u,length=(size_t)units*width;
        if((width==2 && data%2) || (uint64_t)data+length+width>UINT32_MAX)goto bad;
        const uint8_t *payload=read(ctx,data,length+width,0);
        if(!payload || payload[length] || (width==2 && payload[length+1])){why="CFString payload is outside image or unterminated";goto bad;}
        size_t capacity=(size_t)units*(width==2?3u:1u)+1;
        if(capacity>MEMORY_LIMIT-memory-added){why="constant string memory budget exceeded";goto bad;}
        staged[i].identity=(void *)(uintptr_t)(address+16*i);ids[i]=staged[i].identity;
        if(decode(&staged[i],payload,flags,units)){why="invalid ASCII/UTF-16 constant payload";goto bad;}
        added+=staged[i].capacity;
    }
    combined=calloc(constant_count+count,sizeof(*combined));if(!combined)goto bad;
    if(constant_count)memcpy(combined,constants,constant_count*sizeof(*combined));
    memcpy(combined+constant_count,staged,count*sizeof(*combined));
    if(im2_objc_register_static_objects(ids,count,cls)){why="duplicate or conflicting constant identities";goto bad;}
    free(constants);constants=combined;constant_count+=count;memory+=added;free(staged);free(ids);
    if(error && size)error[0]=0;
    return 0;
bad:
    if(staged)for(size_t i=0;i<count;i++)free(staged[i].utf8);
    free(staged);free(combined);free(ids);if(error && size)snprintf(error,size,"%s",why);return -1;
}
int im2_cfstrings_clear(void) {
    void **ids=calloc(constant_count?constant_count:1,sizeof(*ids));if(!ids)return -1;
    for(size_t i=0;i<constant_count;i++)ids[i]=constants[i].identity;
    int result=im2_objc_unregister_static_objects(ids,constant_count);free(ids);if(result)return -1;
    for(size_t i=0;i<constant_count;i++)free(constants[i].utf8);
    free(constants);constants=NULL;constant_count=memory=0;return 0;
}
