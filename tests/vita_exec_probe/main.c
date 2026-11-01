/* SPDX-License-Identifier: MIT. Own ARM/Thumb code only, no guest image.
 * A domain-wide API permission transition is NOT proof of per-page W^X or NX.
 * Last durable checkpoint identifies a faulting call; never run via GPU/VM mocks.
 */
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define DIRECTORY "ux0:data/ironman2_macho/"
#define RESULT_PATH DIRECTORY "vm_execution_probe.json"
#include "probe_code.h"
#define NOT_ATTEMPTED (-2147483647)
int _newlib_heap_size_user=8*1024*1024;
static struct {
 const char *stage,*failure;
 int model,alloc,base_result,close_before,open_first,close_patch,open_second,close_final,free_result;
 int sync_first,sync_second,layout,write_first,write_second,complete;
 uint32_t base,slide,arm_first,thumb_first,layout_first,arm_second,thumb_second,layout_second;
 uint64_t started_us;
} r;
static int report(const char *stage){
 r.stage=stage;FILE *f=fopen(RESULT_PATH,"w");if(!f)return -1;
 fprintf(f,"{\n\"schema\":1,\"probe\":\"native-vm-cache-v3-own-code-only\",\n");
 fprintf(f,"\"execution_origin\":\"unattested_native_runtime\",\"physical_hardware_confirmed\":false,\n");
 fprintf(f,"\"guest_executed\":false,\"per_page_rx_verified\":false,\"closed_domain_nx_tested\":false,\n");
 /* All stage/failure strings are program constants. */
 fprintf(f,"\"stage\":\"%s\",\"failure\":\"%s\",\"model\":%d,\"started_us\":%llu,\n",r.stage,r.failure?r.failure:"",r.model,(unsigned long long)r.started_us);
 fprintf(f,"\"vm_alloc_uid\":%d,\"get_base_result\":%d,\"base\":%u,\"slide\":%u,\"image_span\":%u,\"data_offset\":%u,\"contiguous_layout_match\":%d,\n",r.alloc,r.base_result,r.base,r.slide,SPAN,DATA_VM-TEXT_VM,r.layout);
 fprintf(f,"\"domain_close_before_write\":%d,\"sync_first\":%d,\"domain_open_first\":%d,\"domain_close_before_patch\":%d,\"sync_second\":%d,\"domain_open_second\":%d,\n",r.close_before,r.sync_first,r.open_first,r.close_patch,r.sync_second,r.open_second);
 fprintf(f,"\"initial_bytes_match\":%d,\"patched_bytes_match\":%d,\"arm_first\":%u,\"thumb_first\":%u,\"layout_first\":%u,\"arm_second\":%u,\"thumb_second\":%u,\"layout_second\":%u,\n",r.write_first,r.write_second,r.arm_first,r.thumb_first,r.layout_first,r.arm_second,r.thumb_second,r.layout_second);
 fprintf(f,"\"domain_close_final\":%d,\"free_result\":%d,\"vm_checks_passed\":%d\n}\n",r.close_final,r.free_result,r.complete);
 int ok=!ferror(f);if(fclose(f))ok=0;return ok?0:-1;
}
static uint32_t invoke(void *base,unsigned offset,int thumb){
 return ((uint32_t(*)(void))(((uintptr_t)base+offset)|(unsigned)thumb))();
}
int main(void){
 r.model=sceKernelGetModel();r.started_us=sceKernelGetProcessTimeWide();
 r.alloc=r.base_result=r.close_before=r.open_first=r.close_patch=r.open_second=NOT_ATTEMPTED;
 r.close_final=r.free_result=r.sync_first=r.sync_second=NOT_ATTEMPTED;
 sceIoMkdir(DIRECTORY,0777);
 int domain_open=0,exit_code=1;void *base=NULL;SceUID id=-1;
 if(report("before_vm_allocate"))goto report_failed;
 r.alloc=id=sceKernelAllocMemBlockForVM("im2_own_code_probe",SPAN);
 if(id<0){r.failure="vm_allocate_failed";goto failed;}
 r.base_result=sceKernelGetMemBlockBase(id,&base);
 if(r.base_result<0 || !base){r.failure="vm_base_failed";goto failed;}
 if((uintptr_t)base<TEXT_VM || (uint64_t)(uintptr_t)base+SPAN>UINT32_MAX){r.failure="arm32_address_range_failed";goto failed;}
 r.base=(uint32_t)(uintptr_t)base;r.slide=r.base-TEXT_VM;
 r.layout=(uint64_t)r.slide+DATA_VM==(uint64_t)r.base+DATA_VM-TEXT_VM;
 if(!r.layout){r.failure="contiguous_slide_failed";goto failed;}
 if(report("before_close_domain_for_write"))goto report_failed;
 r.close_before=sceKernelCloseVMDomain();
 if(r.close_before<0){r.failure="domain_close_before_write_failed";goto failed;}
 if(report("before_initial_code_write"))goto report_failed;
 r.write_first=!im2_probe_write_code(base,SPAN,0);
 if(!r.write_first){r.failure="initial_code_write_failed";goto failed;}
 if(report("before_first_cache_sync"))goto report_failed;
 r.sync_first=sceKernelSyncVMDomain(id,base,CODE_BYTES);
 if(r.sync_first<0){r.failure="first_cache_sync_failed";goto failed;}
 if(report("before_first_domain_open"))goto report_failed;
 r.open_first=sceKernelOpenVMDomain();
 if(r.open_first<0){r.failure="first_domain_open_failed";goto failed;}domain_open=1;
 if(report("before_arm_first_call"))goto report_failed;
 r.arm_first=invoke(base,0,0);if(r.arm_first!=42){r.failure="arm_first_return_mismatch";goto failed;}
 if(report("before_thumb_first_call"))goto report_failed;
 r.thumb_first=invoke(base,0x40,1);if(r.thumb_first!=43){r.failure="thumb_first_return_mismatch";goto failed;}
 if(report("before_layout_first_call"))goto report_failed;
 r.layout_first=invoke(base,0x80,0);if(r.layout_first!=0x13579bdfu){r.failure="layout_first_return_mismatch";goto failed;}
 if(report("before_close_domain_for_patch"))goto report_failed;
 r.close_patch=sceKernelCloseVMDomain();
 if(r.close_patch<0){r.failure="domain_close_before_patch_failed";goto failed;}domain_open=0;
 if(report("before_patch_code_write"))goto report_failed;
 r.write_second=!im2_probe_write_code(base,SPAN,1);if(!r.write_second){r.failure="patch_code_write_failed";goto failed;}
 if(report("before_second_cache_sync"))goto report_failed;
 r.sync_second=sceKernelSyncVMDomain(id,base,CODE_BYTES);
 if(r.sync_second<0){r.failure="second_cache_sync_failed";goto failed;}
 if(report("before_second_domain_open"))goto report_failed;
 r.open_second=sceKernelOpenVMDomain();
 if(r.open_second<0){r.failure="second_domain_open_failed";goto failed;}domain_open=1;
 if(report("before_arm_patched_call"))goto report_failed;
 r.arm_second=invoke(base,0,0);if(r.arm_second!=77){r.failure="arm_patched_return_mismatch";goto failed;}
 if(report("before_thumb_patched_call"))goto report_failed;
 r.thumb_second=invoke(base,0x40,1);if(r.thumb_second!=78){r.failure="thumb_patched_return_mismatch";goto failed;}
 if(report("before_layout_patched_call"))goto report_failed;
 r.layout_second=invoke(base,0x80,0);if(r.layout_second!=0x2468ace0u){r.failure="layout_patched_return_mismatch";goto failed;}
 exit_code=0;
failed:
 if(domain_open){r.close_final=sceKernelCloseVMDomain();if(r.close_final<0){r.failure="final_domain_close_failed";exit_code=1;}}
 if(id>=0){r.free_result=sceKernelFreeMemBlock(id);if(r.free_result<0){r.failure="vm_free_failed";exit_code=1;}}
 r.complete=exit_code==0;
 if(report(exit_code?"failed":"vm_checks_passed_unattested_hardware"))goto report_failed;
 sceKernelExitProcess(exit_code);return exit_code;
report_failed:
 if(domain_open)sceKernelCloseVMDomain();
 if(id>=0 && r.free_result==NOT_ATTEMPTED)sceKernelFreeMemBlock(id);
 sceKernelExitProcess(2);return 2;
}
