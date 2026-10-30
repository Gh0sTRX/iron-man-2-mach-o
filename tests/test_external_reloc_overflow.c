/* Host-side regression for Mach-O ARM_RELOC_VANILLA external relocation.
 * Build: cc -std=c11 -Wall -Wextra -Werror -Ivendor/ios-loader-boilerplate/source \
 *   tests/test_external_reloc_overflow.c vendor/ios-loader-boilerplate/source/macho_reloc.c -o /tmp/im2-reloc-test
 * Run: /tmp/im2-reloc-test
 */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "macho_reloc.h"
static uint8_t memory[8];
static uint8_t *map(void *ctx,uint32_t va) {
    (void)ctx;return va==0x1000?memory:va==0x1004?memory+4:NULL;
}
static int resolve(void *ctx,uint32_t sym,uint32_t *out) {
    (void)ctx;if(sym!=1)return -1;*out=0xfffffff0u;return 0;
}
static void write32(uint8_t *p,uint32_t v) {
    for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));
}
int main(void) {
    uint8_t rec[16]={0};char error[160];
    write32(rec,0x1000);write32(rec+4,0x0c000001u);
    write32(memory,0x20); /* Positive addend wraps above UINT32_MAX. */
    assert(im2_apply_external_relocs(rec,1,2,map,resolve,NULL,error,sizeof(error))!=0);
    assert(!strcmp(error,"external relocation address/addend overflow"));
    assert(memory[0]==0x20 && memory[1]==0);
    write32(memory,0xffffffe0u); /* Signed -32 is valid. */
    assert(im2_apply_external_relocs(rec,1,2,map,resolve,NULL,error,sizeof(error))==0);
    assert(memory[0]==0xd0 && memory[1]==0xff && memory[2]==0xff && memory[3]==0xff);
    /* Two records: second overflows, first must remain untouched. */
    write32(memory,0xffffffe0u);write32(memory+4,0x20);
    write32(rec+8,0x1004);write32(rec+12,0x0c000001u);
    assert(im2_apply_external_relocs(rec,2,2,map,resolve,NULL,error,sizeof(error))!=0);
    assert(memory[0]==0xe0 && memory[4]==0x20);
    return 0;
}
