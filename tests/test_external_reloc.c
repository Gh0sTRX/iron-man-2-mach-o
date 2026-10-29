#include "macho_reloc.h"
#include "macho_name.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t bytes[16];static unsigned resolves;
static uint8_t *map(void *ctx,uint32_t va) {(void)ctx;return va>=0x1000 && va<=0x100c?bytes+va-0x1000:NULL;}
static int resolve(void *ctx,uint32_t index,uint32_t *out) {(void)ctx;resolves++;if(index==2)return -1;*out=0x80000000+0x100*index;return 0;}
static void put(uint8_t *p,uint32_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);}
static uint32_t get(uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int main(void) {
    const char full_name[16]={'_','_','o','b','j','c','_','c','l','a','s','s','r','e','f','s'};
    assert(im2_macho_name_equal(full_name,"__objc_classrefs"));
    assert(!im2_macho_name_equal(full_name,"__objc_classref"));
    assert(!im2_macho_name_equal(full_name,"__objc_classrefsX"));
    uint8_t records[16],original[16];char error[128];
    put(records,0x1004);put(records+4,0x0c000001);put(records+8,0x1000);put(records+12,0x0c000000);
    put(bytes,8);put(bytes+4,0xfffffffc);memcpy(original,bytes,16);
    assert(!im2_apply_external_relocs(records,2,3,map,resolve,NULL,error,sizeof(error)));
    assert(get(bytes)==0x80000008 && get(bytes+4)==0x800000fc);
    /* Unknown symbol at second entry must not partially bind the first. */
    memcpy(bytes,original,16);put(records+12,0x0c000002);
    assert(im2_apply_external_relocs(records,2,3,map,resolve,NULL,error,sizeof(error)));
    assert(!memcmp(bytes,original,16));put(records+12,0x0c000000);
    uint32_t bad[]={0x0d000000,0x1c000000,0x08000000,0x04000000,0x0c000003};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        resolves=0;put(records+12,bad[i]);
        assert(im2_apply_external_relocs(records,2,3,map,resolve,NULL,error,sizeof(error)));
        assert(!memcmp(bytes,original,16) && resolves==0);
    }
    put(records+12,0x0c000000);
    uint32_t addr[]={0x80001000,0x1001,0x1010,0x1004};
    for(size_t i=0;i<sizeof(addr)/sizeof(addr[0]);i++) {
        resolves=0;put(records+8,addr[i]);
        assert(im2_apply_external_relocs(records,2,3,map,resolve,NULL,error,sizeof(error)));
        assert(!memcmp(bytes,original,16) && resolves==0);
    }
    assert(im2_apply_external_relocs(NULL,1,3,map,resolve,NULL,error,sizeof(error)));
    assert(!im2_apply_external_relocs(NULL,0,0,map,resolve,NULL,error,sizeof(error)));
    puts("PASS: external relocation addends, rollback, encoding, duplicate, symbol and VM bounds");
}
