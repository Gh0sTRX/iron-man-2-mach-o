/* SPDX-License-Identifier: MIT. Independent implementation, no game or Apple code. */
#include "macho_reloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { uint32_t va,value;uint8_t *slot; } Patch;
static uint32_t read32(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int order(const void *a,const void *b) {
    uint32_t x=((const Patch *)a)->va,y=((const Patch *)b)->va;return (x>y)-(x<y);
}
int im2_apply_external_relocs(const void *records,size_t count,uint32_t ns,
    Im2RelocMap map,Im2RelocResolve resolve,void *ctx,char *error,size_t error_size) {
    const char *why=NULL;Patch *patches=NULL;
    if(count>1000000 || (count && !records) || !map || !resolve) {why="invalid relocation arguments";goto bad;}
    patches=calloc(count?count:1,sizeof(*patches));if(!patches){why="relocation plan allocation failed";goto bad;}
    /* Validate every encoding/index/address before invoking even the resolver. */
    for(size_t i=0;i<count;i++) {
        const uint8_t *r=(const uint8_t *)records+8*i;uint32_t va=read32(r),bits=read32(r+4);
        if((va&0x80000003u) || (bits>>28) || !(bits&0x08000000u) ||
           (bits&0x01000000u) || (bits&0x06000000u)!=0x04000000u) {
            why="need aligned, non-scattered, external absolute ARM_RELOC_VANILLA32";goto bad;
        }
        if((bits&0xffffffu)>=ns) {why="external relocation symbol index outside table";goto bad;}
        uint8_t *slot=map(ctx,va);if(!slot){why="external relocation outside mapped image";goto bad;}
        patches[i]=(Patch){va,read32(slot),slot};
    }
    /* Check duplicate destinations before resolving anything. */
    qsort(patches,count,sizeof(*patches),order);
    for(size_t i=1;i<count;i++) if(patches[i-1].va==patches[i].va) {why="duplicate external relocation address";goto bad;}
    /* Preserve table order for symbol lookup; sort only again before commit is unnecessary. */
    for(size_t i=0;i<count;i++) {
        const uint8_t *r=(const uint8_t *)records+8*i;uint32_t va=read32(r),address=0;
        if(resolve(ctx,read32(r+4)&0xffffffu,&address)) {why="unresolved required external symbol";goto bad;}
        size_t lo=0,hi=count;
        while(lo<hi) {size_t mid=lo+(hi-lo)/2;if(patches[mid].va<va)lo=mid+1;else hi=mid;}
        patches[lo].value+=address; /* Modulo 2^32; preserves negative and vtable+8 addends. */
    }
    for(size_t i=0;i<count;i++) {
        uint32_t v=patches[i].value;uint8_t *p=patches[i].slot;
        p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);
    }
    free(patches);if(error && error_size)error[0]=0;return 0;
bad:
    free(patches);if(error && error_size)snprintf(error,error_size,"%s",why);return -1;
}
