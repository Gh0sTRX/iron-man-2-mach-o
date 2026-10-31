/* SPDX-License-Identifier: MIT. Real mixed atexit/__cxa_atexit LIFO registry.
 * Callbacks run outside the lock and can register more callbacks. Nested/concurrent
 * drains and pathological self-registration are rejected, not silently ignored.
 */
#include "exit_registry.h"
#include "imports.h"
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#define MAX_PENDING 4096u
#define MAX_RUN 65536u
typedef struct Entry {uintptr_t function,argument,dso;int has_argument;struct Entry *next;} Entry;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static Entry *head;
static size_t pending;
static int running;
static Im2ExitCall invoke;
static void *invoke_context;
int im2_exit_init(Im2ExitCall call,void *context){
 if(!call){errno=EINVAL;return -1;}
 pthread_mutex_lock(&mutex);
 if(head || running){pthread_mutex_unlock(&mutex);errno=EBUSY;return -1;}
 invoke=call;invoke_context=context;pthread_mutex_unlock(&mutex);return 0;
}
int im2_exit_register(uintptr_t function,uintptr_t argument,uintptr_t dso,int has_argument){
 if(!function){errno=EINVAL;return -1;}
 Entry *entry=malloc(sizeof(*entry));if(!entry){errno=ENOMEM;return -1;}
 *entry=(Entry){function,argument,dso,!!has_argument,NULL};
 pthread_mutex_lock(&mutex);
 if(!invoke || pending==MAX_PENDING){int error=invoke?ENOMEM:EINVAL;pthread_mutex_unlock(&mutex);free(entry);errno=error;return -1;}
 entry->next=head;head=entry;pending++;pthread_mutex_unlock(&mutex);return 0;
}
size_t im2_exit_pending(void){pthread_mutex_lock(&mutex);size_t count=pending;pthread_mutex_unlock(&mutex);return count;}
int im2_exit_run(void){
 pthread_mutex_lock(&mutex);
 if(!invoke || running){int error=invoke?EBUSY:EINVAL;pthread_mutex_unlock(&mutex);errno=error;return -1;}
 running=1;size_t steps=0;
 for(;;){
  if(!head){running=0;pthread_mutex_unlock(&mutex);return 0;}
  if(steps++==MAX_RUN){running=0;pthread_mutex_unlock(&mutex);errno=ELOOP;return -1;}
  Entry *entry=head;head=entry->next;pending--;
  Im2ExitCall call=invoke;void *ctx=invoke_context;
  pthread_mutex_unlock(&mutex);
  uintptr_t function=entry->function,argument=entry->argument;int has_argument=entry->has_argument;
  free(entry);call(ctx,function,argument,has_argument);
  pthread_mutex_lock(&mutex);
 }
}
static int guest_atexit(uintptr_t fn){return im2_exit_register(fn,0,0,0);}
static int guest_cxa_atexit(uintptr_t fn,uintptr_t arg,uintptr_t dso){return im2_exit_register(fn,arg,dso,1);}
const import_entry exit_registry_imports[]={F(atexit,guest_atexit),F(__cxa_atexit,guest_cxa_atexit)};
const int exit_registry_imports_count=sizeof(exit_registry_imports)/sizeof(exit_registry_imports[0]);
