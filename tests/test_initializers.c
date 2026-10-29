#include "macho_init.h"
#include <assert.h>
#include <stdio.h>
static unsigned called;static uint32_t seen[4];
static void invoke(void *ctx,uint32_t fn) {if(ctx)((uint32_t *)ctx)[2]=0xffffffff;seen[called++]=fn;}
int main(void) {
    uint32_t functions[]={0x1000,0,0x1005};
    assert(!im2_run_initializers(functions,3,0x1000,0x100,invoke,NULL));
    assert(called==2 && seen[0]==0x1000 && seen[1]==0x1005);
    called=0;
    assert(!im2_run_initializers(functions,3,0x1000,0x100,invoke,functions));
    assert(called==2 && seen[1]==0x1005);
    uint32_t bad[]={0x1100,0x1002,0x0fff,0xffffffff};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        functions[2]=bad[i];called=0;
        assert(im2_run_initializers(functions,3,0x1000,0x100,invoke,NULL));assert(!called);
    }
    functions[2]=0x10ff;called=0;
    assert(!im2_run_initializers(functions,3,0x1000,0x100,invoke,NULL));assert(called==2);
    functions[2]=0x10fe;called=0; /* ARM instruction straddles text end and misaligned. */
    assert(im2_run_initializers(functions,3,0x1000,0x100,invoke,NULL));assert(!called);
    assert(im2_run_initializers(NULL,1,0x1000,0x100,invoke,NULL));
    assert(im2_run_initializers(functions,3,0x1000,0x100,NULL,NULL));
    assert(im2_run_initializers(NULL,0,0xfffffff0,0x100,invoke,NULL));
    assert(!im2_run_initializers(NULL,0,0x1000,0x100,NULL,NULL));
    puts("PASS: all constructors validated before callbacks; ARM/Thumb bounds and alignment");
}
