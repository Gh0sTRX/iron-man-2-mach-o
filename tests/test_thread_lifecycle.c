/* SPDX-License-Identifier: MIT. Real host threads + cancellation, controlled errors. */
#include "thread_lifecycle.h"
#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
static int join_error,cancel_error,type_error,type_calls;
int __real_pthread_join(pthread_t,void **);
int __real_pthread_cancel(pthread_t);
int __real_pthread_setcanceltype(int,int *);
int __wrap_pthread_join(pthread_t t,void **out){return join_error?join_error:__real_pthread_join(t,out);}
int __wrap_pthread_cancel(pthread_t t){return cancel_error?cancel_error:__real_pthread_cancel(t);}
int __wrap_pthread_setcanceltype(int type,int *old){type_calls++;return type_error?type_error:__real_pthread_setcanceltype(type,old);}
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static int ready;
static atomic_int cleaned;
static void cleanup(void *unused){(void)unused;atomic_fetch_add(&cleaned,1);}
static void *normal(void *value){return value;}
static void *cancellable(void *unused){
 (void)unused;int32_t old=-1;assert(!im2_thread_setcanceltype(2,&old) && old==2);
 pthread_cleanup_push(cleanup,NULL);
 pthread_mutex_lock(&lock);ready=1;pthread_cond_signal(&changed);pthread_mutex_unlock(&lock);
 for(;;)im2_thread_testcancel();
 pthread_cleanup_pop(1);
 return NULL;
}
int main(void){
 uint32_t unchanged=0x12345678;assert(im2_thread_join((pthread_t)0,&unchanged)==3 && unchanged==0x12345678);
 assert(im2_thread_cancel((pthread_t)0)==3);
 int32_t old=-1;assert(!im2_thread_setcanceltype(0,&old) && old==2);
 assert(!im2_thread_setcanceltype(2,&old) && old==0);
 assert(!im2_thread_setcanceltype(2,NULL));
 int before=type_calls;old=77;assert(im2_thread_setcanceltype(1,&old)==22 && old==77 && type_calls==before);
 type_error=EINVAL;assert(im2_thread_setcanceltype(0,&old)==22 && old==77);type_error=0;
 pthread_t thread;struct {uint32_t value,guard;} out={0,0x12345678};
 assert(!pthread_create(&thread,NULL,normal,(void *)(uintptr_t)0xdeadbeef));
 join_error=EDEADLK;out.value=0xaabbccdd;assert(im2_thread_join(thread,&out.value)==11 && out.value==0xaabbccdd && out.guard==0x12345678);
 join_error=EAGAIN;assert(im2_thread_join(thread,&out.value)==35 && out.value==0xaabbccdd);
 join_error=ENOTSUP;assert(im2_thread_join(thread,&out.value)==45 && out.value==0xaabbccdd);
 join_error=0;assert(!im2_thread_join(thread,&out.value) && out.value==0xdeadbeef && out.guard==0x12345678);
 assert(!pthread_create(&thread,NULL,normal,NULL));assert(!im2_thread_join(thread,NULL));
 assert(!pthread_create(&thread,NULL,cancellable,NULL));
 pthread_mutex_lock(&lock);while(!ready)pthread_cond_wait(&changed,&lock);pthread_mutex_unlock(&lock);
 cancel_error=ESRCH;assert(im2_thread_cancel(thread)==3);cancel_error=0;
 assert(!im2_thread_cancel(thread));out.value=0;
 assert(!im2_thread_join(thread,&out.value) && out.value==1 && out.guard==0x12345678);
 assert(atomic_load(&cleaned)==1);
 puts("PASS: real join/result, deferred cancellation/cleanup, Darwin type/sentinel/error translations, 4-byte result guard and failure preservation");
}
