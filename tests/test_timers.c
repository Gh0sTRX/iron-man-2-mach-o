/* SPDX-License-Identifier: MIT. Real registered timer IMPs, deterministic clock; no Vita/game execution. */
#define _POSIX_C_SOURCE 200809L
#include "timers.h"
#include "uikit_loop.h"
#include "foundation.h"
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static uint64_t now;
static unsigned draws,with_timer,guest_calls,mutations,polls,waits;
static int manual_fire;
static void *last_temp,*other,*spawned,*target,*info;
static uint64_t clock_us(void *ctx){(void)ctx;return now;}
static void failure(const char *why){(void)why;_exit(73);}
static Im2Imp method(void *o,const char *s){Im2ObjcTarget t;assert(!im2_objc_lookup_target(o,s,NULL,&t));assert(t.types && !t.guest);return t.imp;}
static void send_void(void *o,const char *s){((void(*)(void *,const char *))method(o,s))(o,s);}
static uintptr_t send(void *o,const char *s){return ((uintptr_t(*)(void *,const char *))method(o,s))(o,s);}
static void *timer(double seconds,const char *sel,int repeat,void *receiver){
 uint64_t bits;memcpy(&bits,&seconds,8);void *cls=im2_objc_class_named("NSTimer");
 const char *factory="scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:";
 typedef uintptr_t(*Factory)(void *,const char *,uint32_t,uint32_t,void *,const char *,void *,uint32_t);
 return (void *)((Factory)method(cls,factory))(cls,factory,(uint32_t)bits,(uint32_t)(bits>>32),receiver,sel,info,(uint32_t)repeat);
}
static void draw(void *o,const char *s){assert(o==target && !strcmp(s,"draw"));draws++;if(!manual_fire){size_t nested;assert(im2_timers_poll(&nested)==-1);}last_temp=im2_string_create("frame temporary",1);assert(last_temp);}
static void stop_self(void *o,const char *s,void *t){assert(o==target && !strcmp(s,"stop:"));with_timer++;assert(send(t,"userInfo")== (uintptr_t)info);send_void(t,"invalidate");}
static void mutate(void *o,const char *s,void *t){(void)t;assert(o==target && !strcmp(s,"mutate:"));mutations++;send_void(other,"invalidate");spawned=timer(.0001,"draw",0,target);assert(spawned);}
static void guest_never_direct(void){assert(!"guest IMP must use the ARM bridge");}
static uint32_t guest_bridge(void *imp,uint32_t self,uint32_t selector,uint32_t arg,uint32_t last){assert(imp==(void *)guest_never_direct && self && selector && !arg && !last);guest_calls++;return 0;}
static void expect_failure(int which){
 pid_t child=fork();assert(child>=0);if(!child){
  if(which==0)(void)timer(NAN,"draw",1,target);
  else if(which==1)(void)timer(.1,"bad",1,target);
  else if(which==2){now=UINT64_MAX;(void)timer(1,"draw",1,target);}
  else if(which==3){for(int i=0;i<65;i++)assert(timer(1,"draw",1,target));}
  else (void)timer(31536001.,"draw",1,target);
  _exit(74);
 }
 int status;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==73);
}
static void *worker(void *unused){(void)unused;size_t fired;uint32_t delay;assert(im2_timers_poll(&fired)==-1 && im2_timers_delay(1,&delay)==-1 && im2_timers_cancel_all()==-1);return NULL;}
static int poll_input(void *ctx){(void)ctx;polls++;return 0;}
static int wait_clock(void *ctx,uint32_t us){(void)ctx;assert(us>0 && us<=10000);waits++;now+=us;return 0;}
static int should_stop(void *ctx){(void)ctx;return polls==5;}
int main(void){
 Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);assert(root);
 assert(im2_objc_register_class("NSAutoreleasePool",root,sizeof(Im2ObjcObject),NULL,0,NULL,0));
 Im2FoundationConfig cfg={"/tmp",NULL,NULL,NULL};assert(!im2_foundation_init(root,&cfg));
 assert(!im2_timers_init(root,(Im2TimerConfig){clock_us,NULL,guest_bridge,failure}));
 assert(im2_timers_init(root,(Im2TimerConfig){clock_us,NULL,guest_bridge,failure}));
 const Im2MethodSpec im[]={{"draw","v8@0:4",(Im2Imp)draw,0},{"stop:","v12@0:4@8",(Im2Imp)stop_self,0},{"mutate:","v12@0:4@8",(Im2Imp)mutate,0},{"bad","I@:",(Im2Imp)draw,0}};
 Im2ObjcClass *cls=im2_objc_register_class("TimerTarget",root,sizeof(Im2ObjcObject),im,4,NULL,0);assert(cls);
 target=im2_objc_alloc(cls,NULL);info=im2_string_create("payload",0);assert(target && info);
 assert(!timer(.1,"draw",0,target)); /* No autorelease pool: no scheduled leak. */
 assert(!im2_timers_scheduled() && im2_objc_retain_count(target)==1);
 void *pool=im2_objc_alloc(root,NULL);assert(pool && !im2_objc_pool_push(pool));
 void *repeating=timer(1./60.,"draw",1,target);assert(repeating && im2_objc_retain_count(target)==2);
 uint64_t bits=((uint64_t(*)(void *,const char *))method(repeating,"timeInterval"))(repeating,"timeInterval");double seconds;memcpy(&seconds,&bits,8);assert(seconds==1./60.);
 uint32_t delay;assert(!im2_timers_delay(100000,&delay) && delay==16667);
 manual_fire=1;send_void(repeating,"fire");manual_fire=0;assert(draws==1);draws=0;
 assert(!im2_timers_delay(100000,&delay) && delay==16667);
 size_t fired;now=16666;assert(!im2_timers_poll(&fired) && !fired);now=16667;assert(!im2_timers_poll(&fired) && fired==1 && draws==1);
 assert(!im2_objc_retain_count(last_temp));now=1000000;assert(!im2_timers_poll(&fired) && fired==1 && draws==2);
 assert(!im2_timers_delay(100000,&delay) && delay>0 && delay<=16667); /* Missed ticks coalesce. */
 send_void(repeating,"invalidate");assert(!send(repeating,"isValid") && im2_objc_retain_count(target)==1);
 void *once=timer(1,"draw",0,target);assert(once);now=2000000;assert(!im2_timers_poll(&fired) && fired==1 && draws==3);
 assert(!send(once,"isValid") && !im2_timers_scheduled() && send(once,"userInfo")== (uintptr_t)info);
 void *high_bool=timer(.000001,"draw",0x100,target);assert(high_bool);now++;assert(!im2_timers_poll(&fired) && fired==1 && !send(high_bool,"isValid"));
 void *self=timer(.1,"stop:",1,target);assert(self);now+=100001;assert(!im2_timers_poll(&fired) && fired==1 && with_timer==1 && !send(self,"isValid"));
 void *first=timer(.1,"mutate:",0,target);other=timer(.1,"draw",0,target);assert(first && other);
 now+=100001;unsigned before=draws;assert(!im2_timers_poll(&fired) && fired==1 && mutations==1 && draws==before && spawned);
 now+=100;assert(!im2_timers_poll(&fired) && fired==1 && draws==before+1);
 const Im2MethodSpec guest_im[]={{"draw","v8@0:4",(Im2Imp)guest_never_direct,0}};
 Im2ObjcClass *guest=im2_objc_register_foreign_class("OriginalStyleTarget",(void *)0x25000000,(void *)0x25000020,root,sizeof(Im2ObjcObject),guest_im,1,NULL,0);assert(guest);
 void *guest_object=im2_objc_alloc(guest,NULL);assert(guest_object && timer(.01,"draw",0,guest_object));now+=10000;assert(!im2_timers_poll(&fired) && fired==1 && guest_calls==1);assert(!im2_objc_release(guest_object));
 for(int i=0;i<5;i++)expect_failure(i);
 pthread_t thread;assert(!pthread_create(&thread,NULL,worker,NULL) && !pthread_join(thread,NULL));
 assert(timer(-1,"draw",0,target)); /* Nonpositive interval clamps to 100 us. */
 now+=100;assert(!im2_timers_poll(&fired) && fired==1);
 assert(timer(.02,"draw",1,target));Im2UIKitLoop loop={NULL,poll_input,wait_clock,should_stop,10000};
 assert(!im2_uikit_loop_run(&loop) && polls==5 && waits==5);assert(im2_uikit_loop_run(NULL));
 assert(!im2_timers_cancel_all() && !im2_timers_scheduled());
 assert(!im2_timers_poll(&fired) && !fired);now--;assert(im2_timers_poll(&fired)==-1);now++;
 assert(!im2_objc_pool_drain(pool));assert(im2_objc_retain_count(target)==1 && im2_objc_retain_count(info)==1);
 assert(!im2_objc_release(target) && !im2_objc_release(info));
 puts("PASS: raw SoftFP interval words, deadline/coalescing, native/guest typed callbacks, cancellation snapshots, per-turn pools, lifetime, thread/clock guards and bounded loop waits");
}
