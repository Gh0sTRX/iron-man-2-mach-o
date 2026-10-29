/* Vita ARM dynamic-execution probe. No original game assets. */
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/ctrl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define PROBE_SIZE 0x1000
#define RESULT_PATH "ux0:data/im2_exec_probe.txt"
__attribute__((noinline)) static int compiled_arm_test(void) { return 42; }
static void checkpoint(const char *msg) {
 FILE *f=fopen(RESULT_PATH,"a");
 if(f){fprintf(f,"%s\n",msg);fclose(f);}
}
int main(void){
 FILE *f=fopen(RESULT_PATH,"w");
 if(f){fprintf(f,"IM2 executable memory probe v2\n");fclose(f);}
 if(compiled_arm_test()!=42){checkpoint("FAIL_COMPILED_ARM");sceKernelExitProcess(1);return 1;}
 checkpoint("PASS_COMPILED_ARM");
 SceUID id=sceKernelAllocMemBlockForVM("im2_exec_probe",PROBE_SIZE);
 if(id<0){checkpoint("FAIL_VM_ALLOC");sceKernelExitProcess(2);return 2;}
 void *base=NULL;
 if(sceKernelGetMemBlockBase(id,&base)<0||!base){
  checkpoint("FAIL_VM_BASE");sceKernelFreeMemBlock(id);sceKernelExitProcess(3);return 3;
 }
 const uint32_t code[2]={0xE3A0002Au,0xE12FFF1Eu};
 memcpy(base,code,sizeof code);
 if(memcmp(base,code,sizeof code)!=0){
  checkpoint("FAIL_VM_WRITE");sceKernelFreeMemBlock(id);sceKernelExitProcess(4);return 4;
 }
 checkpoint("PASS_VM_WRITE");
 checkpoint("WARNING_ICACHE_NOT_FLUSHED_RESULT_INCONCLUSIVE");
 checkpoint("WAITING_FOR_LTRIGGER_RTRIGGER_START");
 int approved=0;
 for(int i=0;i<900;i++){
  SceCtrlData pad;memset(&pad,0,sizeof pad);
  sceCtrlPeekBufferPositive(0,&pad,1);
  if((pad.buttons&(SCE_CTRL_LTRIGGER|SCE_CTRL_RTRIGGER|SCE_CTRL_START))==
      (SCE_CTRL_LTRIGGER|SCE_CTRL_RTRIGGER|SCE_CTRL_START)){approved=1;break;}
  sceKernelDelayThread(10000);
 }
 if(!approved){checkpoint("SKIP_DYNAMIC_EXEC_NO_CONFIRMATION");sceKernelFreeMemBlock(id);sceKernelExitProcess(0);return 0;}
 /* Without an instruction-cache flush, even a successful return is preliminary.
  * Prefer emulator first; this call may fault and terminate the process. */
 checkpoint("ATTEMPT_DYNAMIC_EXEC_UNFLUSHED_ICACHE");
 int (*fn)(void)=(int(*)(void))base;
 int value=fn();
 checkpoint(value==42?"PRELIMINARY_DYNAMIC_EXEC_42":"FAIL_DYNAMIC_EXEC_RETURN");
 sceKernelFreeMemBlock(id);sceKernelExitProcess(value==42?0:5);
 return value==42?0:5;
}
