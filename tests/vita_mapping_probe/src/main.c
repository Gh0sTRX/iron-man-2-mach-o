#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/processmgr.h>
#include <stdint.h>
#include <stdio.h>
#define TEXT_VM 0x1000u
#define DATA_VM 0x4B9000u
#define DATA_SIZE 0x43000u
#define SPAN (DATA_VM + DATA_SIZE - TEXT_VM)
int main(void) {
 FILE *f=fopen("ux0:data/im2_mapping_probe.txt","w");
 SceUID uid=sceKernelAllocMemBlockForVM("im2_probe",SPAN);
 if(uid<0){if(f){fprintf(f,"FAIL allocation=%d\n",uid);fclose(f);}sceKernelExitProcess(1);return 1;}
 void *base=0;
 int rc=sceKernelGetMemBlockBase(uid,&base);
 if(rc<0||!base||(uintptr_t)base<TEXT_VM||(uintptr_t)base>UINT32_MAX-SPAN){
  if(f){fprintf(f,"FAIL base rc=%d ptr=%p\n",rc,base);fclose(f);}
  sceKernelFreeMemBlock(uid);sceKernelExitProcess(2);return 2;
 }
 uintptr_t slide=(uintptr_t)base-TEXT_VM;
 volatile uint32_t *text=(volatile uint32_t*)base;
 volatile uint32_t *data=(volatile uint32_t*)(slide+DATA_VM);
 *text=0x12345678u;*data=0x89ABCDEFu;
 int ok=(*text==0x12345678u && *data==0x89ABCDEFu);
 if(f){fprintf(f,"base=%p slide=0x%08X text=0x%08X data=0x%08X\nRESULT=%s\nEXECUTE_PERMISSION=NOT_TESTED\n",base,(unsigned)slide,*text,*data,ok?"PASS_RW_LAYOUT":"FAIL_RW_LAYOUT");fclose(f);}
 sceKernelFreeMemBlock(uid);sceKernelExitProcess(ok?0:3);return ok?0:3;
}
