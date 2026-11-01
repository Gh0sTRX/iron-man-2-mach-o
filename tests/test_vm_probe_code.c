/* ASan/UBSan checks of the exact diagnostic generator. No dynamic host execution. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include "vita_exec_probe/probe_code.h"
int main(void){
 uint8_t *base=mmap(NULL,SPAN,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
 assert(base!=MAP_FAILED);memset(base,0xa5,SPAN);
 const uint32_t value=1;
 assert(im2_probe_put(base,SIZE_MAX,&value,4)==-1);
 assert(im2_probe_put(base,CODE_BYTES-1,&value,4)==-1);
 assert(im2_probe_put(base,0,&value,SIZE_MAX)==-1);
 assert(im2_probe_write_code(NULL,SPAN,0)==-1);
 assert(im2_probe_write_code(base,SPAN-1,0)==-1);
 assert(im2_probe_write_code((void *)(uintptr_t)0xfffff000,SPAN,0)==-1);
 assert(im2_probe_write_code(base,SPAN,2)==-1);
 for(size_t i=0;i<SPAN;i++)assert(base[i]==0xa5);
 for(int patched=0;patched<2;patched++){
  assert(!im2_probe_write_code(base,SPAN,patched));
  /* Full allocation scan proves generator modifies only declared code/data slots. */
  for(size_t i=0;i<SPAN;i++)if(!(i<8 || (i>=0x40 && i<0x44) || (i>=0x80 && i<0x90) || (i>=DATA_VM-TEXT_VM && i<DATA_VM-TEXT_VM+4)))assert(base[i]==0xa5);
  char name[128];snprintf(name,sizeof name,"build/host-runtime/vm-own-code-%d.bin",patched);
  FILE *f=fopen(name,"wb");assert(f);uint32_t address=(uint32_t)(uintptr_t)base;
  assert(fwrite(&address,4,1,f)==1);assert(fwrite(base,1,SPAN,f)==SPAN);assert(!fclose(f));
 }
 assert(!munmap(base,SPAN));puts("PASS: own VM probe generator bounds and exact modified ranges (host, no VM API execution)");
}
