/* SPDX-License-Identifier: MIT. Own bounded ARM32 diagnostic code, no guest input. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define TEXT_VM 0x1000u
#define DATA_VM 0x4b9000u
#define DATA_SIZE 0x43000u
#define SPAN (DATA_VM + DATA_SIZE - TEXT_VM)
#define CODE_BYTES 0x100u
/* Bounded copies of our own literal instructions. No external binary input. */
static inline int im2_probe_put(void *base,size_t offset,const void *bytes,size_t count){
 if(!base || !bytes || offset>CODE_BYTES || count>CODE_BYTES-offset)return -1;
 memcpy((uint8_t *)base+offset,bytes,count);
 return memcmp((uint8_t *)base+offset,bytes,count)?-1:0;
}
static inline int im2_probe_write_code(void *base,size_t capacity,int patched){
 if(!base || capacity<SPAN || (uint64_t)(uintptr_t)base+SPAN>UINT32_MAX ||
    (patched!=0 && patched!=1))return -1;
 const uint32_t arm[]={patched?0xe3a0004du:0xe3a0002au,0xe12fff1eu}; /* mov r0,42/77; bx lr */
 const uint16_t thumb[]={patched?0x204eu:0x202bu,0x4770u}; /* movs r0,43/78; bx lr */
 const uint32_t data_address=(uint32_t)(uintptr_t)base+DATA_VM-TEXT_VM;
 /* ldr r1,[pc,#4]; ldr r0,[r1]; bx lr; own data address */
 const uint32_t layout[]={0xe59f1004u,0xe5910000u,0xe12fff1eu,data_address};
 if(im2_probe_put(base,0,arm,sizeof arm) || im2_probe_put(base,0x40,thumb,sizeof thumb) || im2_probe_put(base,0x80,layout,sizeof layout))return -1;
 uint32_t *data=(uint32_t *)((uint8_t *)base+DATA_VM-TEXT_VM);
 *data=patched?0x2468ace0u:0x13579bdfu;
 return *data==(patched?0x2468ace0u:0x13579bdfu)?0:-1;
}
