/* SPDX-License-Identifier: MIT. Real callbacks, concurrent registration and LIFO. */
#include "exit_registry.h"
#include "imports.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
static uintptr_t events[8192];static unsigned count,loop_count;static int again;
static void invoke(void *ctx,uintptr_t fn,uintptr_t arg,int has){(void)ctx;if(has)((void(*)(uintptr_t))fn)(arg);else ((void(*)(void))fn)();}
static void mark(uintptr_t value){assert(count<8192);events[count++]=value;}
static void one(void){mark(1);}static void late(void){mark(77);}
static void dynamic(void){mark(30);assert(im2_exit_run()==-1 && errno==EBUSY);assert(!im2_exit_register((uintptr_t)late,0,0,0));}
static void self_register(void){loop_count++;if(again)assert(!im2_exit_register((uintptr_t)self_register,0,0,0));}
static void *register_thread(void *arg){uintptr_t n=(uintptr_t)arg;for(unsigned i=0;i<32;i++)assert(!im2_exit_register((uintptr_t)mark,n*32+i,n,1));return NULL;}
int main(void){
 assert(im2_exit_run()==-1 && errno==EINVAL);assert(im2_exit_init(NULL,NULL)==-1);
 assert(!im2_exit_init(invoke,NULL));assert(im2_exit_register(0,0,0,0)==-1 && errno==EINVAL);
 int (*atexit_fn)(uintptr_t)=(int(*)(uintptr_t))exit_registry_imports[0].addr;
 int (*cxa)(uintptr_t,uintptr_t,uintptr_t)=(int(*)(uintptr_t,uintptr_t,uintptr_t))exit_registry_imports[1].addr;
 assert(!atexit_fn((uintptr_t)one));assert(!cxa((uintptr_t)mark,10,123));assert(!atexit_fn((uintptr_t)dynamic));
 assert(im2_exit_pending()==3 && im2_exit_init(invoke,NULL)==-1 && errno==EBUSY);
 assert(!im2_exit_run() && !im2_exit_pending());assert(count==4 && events[0]==30 && events[1]==77 && events[2]==10 && events[3]==1);
 assert(!im2_exit_run() && count==4);
 count=0;pthread_t threads[8];for(uintptr_t i=0;i<8;i++)assert(!pthread_create(threads+i,NULL,register_thread,(void*)i));
 for(unsigned i=0;i<8;i++)assert(!pthread_join(threads[i],NULL));
 assert(im2_exit_pending()==256 && !im2_exit_run() && count==256);
 unsigned char seen[256]={0};for(unsigned i=0;i<count;i++){assert(events[i]<256 && !seen[events[i]]);seen[events[i]]=1;}
 count=0;for(unsigned i=0;i<4096;i++)assert(!cxa((uintptr_t)mark,i,456));
 assert(cxa((uintptr_t)mark,0,0)==-1 && errno==ENOMEM && im2_exit_pending()==4096);
 assert(!im2_exit_run() && count==4096);for(unsigned i=0;i<4096;i++)assert(events[i]==4095-i);
 again=1;assert(!atexit_fn((uintptr_t)self_register));assert(im2_exit_run()==-1 && errno==ELOOP && loop_count==65536 && im2_exit_pending()==1);
 again=0;assert(!im2_exit_run() && loop_count==65537 && !im2_exit_pending());
 puts("PASS: mixed guest exit callbacks, LIFO/exactly-once, registration from callbacks, reentry rejection, 8-thread registration, queue and run budgets");
}
