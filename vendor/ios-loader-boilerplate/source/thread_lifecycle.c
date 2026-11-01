/* SPDX-License-Identifier: MIT. Independent scalar ARM32 pthread adapters.
 * Darwin: deferred=2, asynchronous=0, canceled=(void*)1. Vita PTE/newlib:
 * deferred=1, asynchronous=0, canceled=(void*)-1. Do not pass host enums through.
 */
#include "thread_lifecycle.h"
#include <errno.h>
#include <stddef.h>
#if defined(__arm__)
_Static_assert(sizeof(pthread_t)==4 && sizeof(void *)==4,"ARM32 pthread ABI required");
#endif
int im2_thread_error(int error){
 switch(error){
 case 0:return 0;
 case EPERM:return 1;
 case ESRCH:return 3;
 case EDEADLK:return 11;
 case ENOMEM:return 12;
 case EBUSY:return 16;
 case EINVAL:return 22;
 case EAGAIN:return 35;
 case ENOTSUP:return 45;
 case ECANCELED:return 89;
 default:return 45; /* Unknown target errors are unsupported, never success. */
 }
}
int im2_thread_join(pthread_t thread,uint32_t *guest_result){
 if(!thread)return 3;
 void *result=NULL;
 int error=pthread_join(thread,guest_result?&result:NULL);
 if(!error && guest_result)*guest_result=result==PTHREAD_CANCELED?1u:(uint32_t)(uintptr_t)result;
 return im2_thread_error(error);
}
int im2_thread_cancel(pthread_t thread){return thread?im2_thread_error(pthread_cancel(thread)):3;}
int im2_thread_setcanceltype(int darwin_type,int32_t *old_type){
 int type;
 if(darwin_type==2)type=PTHREAD_CANCEL_DEFERRED;
 else if(darwin_type==0)type=PTHREAD_CANCEL_ASYNCHRONOUS;
 else return 22;
 int old;
 int error=pthread_setcanceltype(type,old_type?&old:NULL);
 if(error)return im2_thread_error(error);
 if(old_type){
  if(old==PTHREAD_CANCEL_DEFERRED)*old_type=2;
  else if(old==PTHREAD_CANCEL_ASYNCHRONOUS)*old_type=0;
  else return 45;
 }
 return 0;
}
void im2_thread_testcancel(void){pthread_testcancel();}
