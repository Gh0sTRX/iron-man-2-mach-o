/* SPDX-License-Identifier: MIT. Real pthread isolation, deterministic per-thread clocks. */
#define _POSIX_C_SOURCE 200809L
#include "runloop.h"
#include "timers.h"
#include "foundation.h"
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static _Thread_local uint64_t ticks;
static _Thread_local unsigned fires,waits;
static _Thread_local int nested,wait_error;
static Im2ObjcClass *root,*target_class;
static void *main_timer;
static pthread_barrier_t barrier;
static uint64_t clock_us(void *unused){(void)unused;return ticks;}
static int wait_us(void *unused,uint32_t us){(void)unused;assert(us && us<=10000);if(wait_error)return -1;ticks+=us;waits++;return 0;}
static void failure(const char *message){(void)message;_exit(73);}
static int run(double seconds,int stop){uint64_t bits;memcpy(&bits,&seconds,8);return im2_cf_runloop_run(im2_cf_default_mode,(uint32_t)bits,(uint32_t)(bits>>32),(uint32_t)stop);}
static Im2Imp method(void *o,const char *name){Im2ObjcTarget t;assert(!im2_objc_lookup_target(o,name,NULL,&t));return t.imp;}
static void callback(void *self,const char *selector){(void)self;(void)selector;fires++;assert(im2_string_create("temporary",1));if(nested)(void)run(.01,0);}
static void *timer(void *target,double seconds,int repeat){
 uint64_t bits;memcpy(&bits,&seconds,8);void *cls=im2_objc_class_named("NSTimer");
 const char *sel="scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:";
 return (void *)((uintptr_t(*)(void *,const char *,uint32_t,uint32_t,void *,const char *,void *,uint32_t))method(cls,sel))
  (cls,sel,(uint32_t)bits,(uint32_t)(bits>>32),target,"tick",NULL,(uint32_t)repeat);
}
typedef struct {void *target,*kept_timer;unsigned fired;} Worker;
static void *worker(void *argument){
 Worker *w=argument;void *pool=im2_objc_alloc(root,NULL);assert(pool && !im2_objc_pool_push(pool));
 w->target=im2_objc_alloc(target_class,NULL);assert(w->target);
 w->kept_timer=timer(w->target,.000010,1);assert(w->kept_timer && im2_objc_retain(w->kept_timer));
 assert(im2_timers_scheduled()==1);int r=pthread_barrier_wait(&barrier);assert(!r || r==PTHREAD_BARRIER_SERIAL_THREAD);
 assert(run(.000025,1)==3 && fires==2 && ticks==25 && waits==3);
 assert(im2_timers_scheduled()==1);w->fired=fires;
 assert(!im2_objc_pool_drain(pool));assert(!im2_objc_release(w->target));
 /* Leave the scheduled timer to the production TLS destructor. A retained
  * timer outlives its owner to check scheduler lifetime and cross-thread release. */
 return NULL;
}
static void *cross_thread_access(void *unused){(void)unused;((void(*)(void *,const char *))method(main_timer,"invalidate"))(main_timer,"invalidate");return NULL;}
static void rejected(int operation){
 pid_t pid=fork();assert(pid>=0);
 if(!pid){
  if(operation==0){void *mode=im2_string_create("unsupported mode",0);(void)im2_cf_runloop_run(mode,0,0,0);}
  if(operation==1)(void)run(NAN,0);
  if(operation==2){nested=1;(void)run(.001,0);}
  if(operation==3){wait_error=1;(void)run(.001,0);}
  if(operation==4){ticks=0;(void)run(0,0);}
  if(operation==5){pthread_t t;assert(!pthread_create(&t,NULL,cross_thread_access,NULL));assert(!pthread_join(t,NULL));}
  _exit(74);
 }
 int status;assert(waitpid(pid,&status,0)==pid);assert(WIFEXITED(status) && WEXITSTATUS(status)==73);
}
int main(void){
 root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);assert(root);
 assert(im2_objc_register_class("NSAutoreleasePool",root,sizeof(Im2ObjcObject),NULL,0,NULL,0));
 assert(!im2_foundation_init(root,&(Im2FoundationConfig){"/tmp",NULL,failure,NULL}));
 assert(!im2_timers_init(root,(Im2TimerConfig){clock_us,NULL,NULL,failure}));
 assert(!im2_runloop_init((Im2RunLoopConfig){NULL,clock_us,wait_us,failure}));
 assert(im2_runloop_init((Im2RunLoopConfig){NULL,clock_us,wait_us,failure}));
 const Im2MethodSpec methods[]={{"tick","v8@0:4",(Im2Imp)callback,0}};
 target_class=im2_objc_register_class("RunLoopTarget",root,sizeof(Im2ObjcObject),methods,1,NULL,0);assert(target_class);
 assert(run(1,1)==1 && !fires && !waits); /* Empty mode finishes; it does not sleep. */
 void *pool=im2_objc_alloc(root,NULL);assert(pool && !im2_objc_pool_push(pool));
 void *target=im2_objc_alloc(target_class,NULL);assert(target);main_timer=timer(target,.000100,1);assert(main_timer);
 assert(run(-1,1)==3 && !fires && !waits);
 assert(run(.000001,1)==3 && ticks==1 && !fires && waits==1); /* Original packed 1us argument. */
 assert(!pthread_barrier_init(&barrier,NULL,3));Worker workers[2]={{0}};pthread_t threads[2];
 for(int i=0;i<2;i++)assert(!pthread_create(threads+i,NULL,worker,workers+i));
 int r=pthread_barrier_wait(&barrier);assert(!r || r==PTHREAD_BARRIER_SERIAL_THREAD);
 size_t count;assert(im2_timers_scheduled()==1 && !im2_timers_poll(&count) && !count && !fires);
 for(int i=0;i<2;i++){
  assert(!pthread_join(threads[i],NULL) && workers[i].fired==2);
  assert(!im2_objc_retain_count(workers[i].target));
  assert(im2_objc_retain_count(workers[i].kept_timer)==1);
  assert(!im2_objc_release(workers[i].kept_timer));
 }
 assert(!pthread_barrier_destroy(&barrier));
 assert(run(.000249,1)==3 && ticks==250 && fires==2); /* Timers do not return HandledSource. */
 for(int i=0;i<6;i++)rejected(i);
 assert(!im2_timers_cancel_all() && !im2_timers_scheduled());
 assert(run(1,0)==1);
 assert(timer(target,.000010,0));assert(run(1,0)==1 && fires==3 && ticks==260);
 assert(!im2_objc_pool_drain(pool) && im2_objc_retain_count(target)==1);
 assert(!im2_objc_release(target));
 puts("PASS: per-thread CF default-mode timers, real concurrent queues/TLS teardown, retained timer lifetime, packed timeout/result semantics, nested/mode/clock/wait rejection");
}
