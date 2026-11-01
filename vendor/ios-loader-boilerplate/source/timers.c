/* SPDX-License-Identifier: MIT. Independent timer scheduler; no Apple/game code.
 * Default-mode selector timers have an independent queue per pthread. */
#include "timers.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#define LIMIT 64u
typedef struct Timer Timer;
typedef struct {
 pthread_t owner;Timer *scheduled[LIMIT];
 int polling,have_clock,closing;uint64_t previous;
 _Atomic unsigned references;
} Scheduler;
struct Timer {
 Im2ObjcClass *isa;void *target,*info;const char *selector;
 uint64_t interval,due,interval_bits;int valid,repeats,scheduled,firing;
 Scheduler *scheduler;
};
static Im2ObjcClass *timer_class;
static Im2TimerConfig config;
static pthread_key_t scheduler_key;
static int scheduler_key_ready;
static void fail(const char *why){if(config.failure)config.failure(why);abort();}
static void release_scheduler(Scheduler *s){if(atomic_fetch_sub(&s->references,1)==1)free(s);}
static Scheduler *scheduler(int create){
 if(!timer_class || !scheduler_key_ready)return NULL;
 Scheduler *s=pthread_getspecific(scheduler_key);
 if(!s && create){s=calloc(1,sizeof(*s));if(!s)return NULL;s->owner=pthread_self();atomic_init(&s->references,1);
  if(pthread_setspecific(scheduler_key,s)){free(s);return NULL;}}
 return s;
}
static int on_timer_thread(Timer *t){Scheduler *s=scheduler(0);return s && t->scheduler==s && pthread_equal(s->owner,pthread_self());}
static int clock_now(uint64_t *now){Scheduler *s=scheduler(1);if(!s)return -1;*now=config.now_us(config.context);if(s->have_clock && *now<s->previous)return -1;s->previous=*now;s->have_clock=1;return 0;}
static const char *skip_digits(const char *p){while(*p>='0' && *p<='9')p++;return p;}
static int arity(const Im2ObjcTarget *t){
 if(!t->imp || t->stret_size || !t->types)return -1;
 const char *p=t->types;if(*p++!='v')return -1;p=skip_digits(p);
 if(*p++!='@')return -1;
 p=skip_digits(p);if(*p++!=':')return -1;
 p=skip_digits(p);
 if(!*p)return 0;
 if(*p++!='@')return -1;
 p=skip_digits(p);return *p?-1:1;
}
static void cleanup(void *o){Timer *t=o;if(t->scheduled)fail("Deallocating a scheduled NSTimer");if(t->target)im2_objc_release(t->target);if(t->info)im2_objc_release(t->info);t->target=t->info=NULL;if(t->scheduler){Scheduler *s=t->scheduler;t->scheduler=NULL;release_scheduler(s);}}
static int invalidate(Timer *t){
 if(!on_timer_thread(t) || !im2_objc_retain(t))return -1;
 Timer **scheduled=t->scheduler->scheduled;
 t->valid=0;void *target=t->target;t->target=NULL;
 for(size_t i=0;i<LIMIT;i++)if(scheduled[i]==t){scheduled[i]=NULL;t->scheduled=0;im2_objc_release(t);break;}
 if(target)im2_objc_release(target);
 return im2_objc_release(t);
}
static void scheduler_destroy(void *value){
 Scheduler *s=value;s->closing=1;
 /* POSIX clears the key before calling its destructor. Rebind only while
  * invalidating our queue; never execute timer callbacks during thread exit. */
 if(pthread_setspecific(scheduler_key,s))fail("Timer scheduler teardown TLS failed");
 for(size_t i=0;i<LIMIT;i++)if(s->scheduled[i] && invalidate(s->scheduled[i]))fail("Timer scheduler teardown failed");
 if(pthread_setspecific(scheduler_key,NULL))fail("Timer scheduler TLS clear failed");
 release_scheduler(s);
}
static int invoke(Timer *t){
 if(!t->valid)return 0;
 if(t->firing)return -1;
 Im2ObjcTarget callback;if(im2_objc_lookup_target(t->target,t->selector,NULL,&callback))return -1;
 int args=arity(&callback);if(args<0 || (callback.guest && !config.guest))return -1;
 void *target=im2_objc_retain(t->target);if(!target || !im2_objc_retain(t)){if(target)im2_objc_release(target);return -1;}
 t->firing=1;
 if(callback.guest)config.guest((void *)callback.imp,(uint32_t)(uintptr_t)target,(uint32_t)(uintptr_t)t->selector,args?(uint32_t)(uintptr_t)t:0,0);
 else if(args)((void(*)(void *,const char *,void *))callback.imp)(target,t->selector,t);
 else ((void(*)(void *,const char *))callback.imp)(target,t->selector);
 t->firing=0;int result=0;
 if(!t->repeats && t->valid)result=invalidate(t);
 im2_objc_release(target);im2_objc_release(t);return result;
}
/* Explicit ARM32 word arguments preserve iOS SoftFP double placement in r2/r3.
 * A native double argument here would instead arrive in d0 under Vita hard-float. */
static uintptr_t factory(void *cls,const char *sel,uint32_t lo,uint32_t hi,
 void *target,const char *selector,void *info,uint32_t repeats){
 (void)sel;Scheduler *s=scheduler(1);
 if(cls!=timer_class || !s || s->closing)fail("NSTimer factory requires a live owning thread and native class");
 Timer **scheduled=s->scheduled;
 uint64_t bits=(uint64_t)hi<<32|lo;double seconds;memcpy(&seconds,&bits,sizeof(seconds));
 if(!isfinite(seconds) || seconds>31536000.)fail("Unsupported NSTimer interval");
 if(seconds<=0){seconds=0.0001;memcpy(&bits,&seconds,sizeof(bits));}
 uint64_t interval=(uint64_t)ceil(seconds*1000000.);if(!interval)interval=1;
 uint64_t now;if(clock_now(&now) || UINT64_MAX-now<interval)fail("NSTimer deadline overflow or invalid clock");
 Im2ObjcTarget callback;const char *interned=im2_objc_sel_register(selector);
 if(!target || !interned || im2_objc_lookup_target(target,interned,NULL,&callback) || arity(&callback)<0 || (callback.guest && !config.guest))fail("Unsupported NSTimer callback signature");
 size_t slot=0;while(slot<LIMIT && scheduled[slot])slot++;if(slot==LIMIT)fail("NSTimer schedule capacity exceeded");
 Timer *t=im2_objc_alloc(timer_class,cleanup);if(!t)return 0;
 t->scheduler=s;atomic_fetch_add(&s->references,1);
 t->target=im2_objc_retain(target);t->info=info?im2_objc_retain(info):NULL;
 if(!t->target || (info && !t->info)){im2_objc_release(t);return 0;}
 t->selector=interned;t->interval=interval;t->due=now+interval;t->interval_bits=bits;t->valid=1;t->repeats=(uint8_t)repeats!=0;
 if(!im2_objc_autorelease(t)){im2_objc_release(t);return 0;}
 scheduled[slot]=im2_objc_retain(t);t->scheduled=1;return (uintptr_t)t;
}
static void timer_invalidate(void *o,const char *sel){(void)sel;if(invalidate(o))fail("Invalid NSTimer invalidation");}
static void timer_fire(void *o,const char *sel){(void)sel;if(!on_timer_thread(o) || invoke(o))fail("Unsupported or reentrant NSTimer fire");}
static uint32_t timer_valid(void *o,const char *sel){(void)sel;if(!on_timer_thread(o))fail("Cross-thread NSTimer access");return ((Timer *)o)->valid;}
static uintptr_t timer_info(void *o,const char *sel){(void)sel;if(!on_timer_thread(o))fail("Cross-thread NSTimer access");return (uintptr_t)((Timer *)o)->info;}
static uint64_t timer_interval(void *o,const char *sel){(void)sel;if(!on_timer_thread(o))fail("Cross-thread NSTimer access");return ((Timer *)o)->interval_bits;}
static void timer_dealloc(void *o,const char *sel){(void)sel;cleanup(o);}
#define M(s,t,f) {s,t,(Im2Imp)(f),0}
int im2_timers_init(Im2ObjcClass *root,Im2TimerConfig cfg){
 if(timer_class || !cfg.now_us || !im2_objc_class_named("NSAutoreleasePool"))return -1;
 const Im2ObjcClass *r=im2_objc_class_metadata(root);if(!r || r->data->ro->flags&1)return -1;
 const Im2MethodSpec im[]={M("invalidate","v@:",timer_invalidate),M("fire","v@:",timer_fire),M("isValid","c@:",timer_valid),M("userInfo","@@:",timer_info),M("timeInterval","d@:",timer_interval),M("dealloc","v@:",timer_dealloc)};
 const Im2MethodSpec cm[]={M("scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:","@@:d@:@c",factory)};
 if(pthread_key_create(&scheduler_key,scheduler_destroy))return -1;
 scheduler_key_ready=1;
 timer_class=im2_objc_register_class("NSTimer",root,sizeof(Timer),im,6,cm,1);
 if(!timer_class){pthread_key_delete(scheduler_key);scheduler_key_ready=0;return -1;}
 config=cfg;return 0;
}
size_t im2_timers_scheduled(void){Scheduler *s=scheduler(0);if(!s)return 0;size_t n=0;for(size_t i=0;i<LIMIT;i++)n+=s->scheduled[i]!=NULL;return n;}
int im2_timers_cancel_all(void){Scheduler *s=scheduler(1);if(!s)return -1;for(size_t i=0;i<LIMIT;i++)if(s->scheduled[i] && invalidate(s->scheduled[i]))return -1;return 0;}
int im2_timers_delay(uint32_t cap,uint32_t *delay){
 uint64_t now;if(!delay || clock_now(&now))return -1;*delay=cap;
 Timer **scheduled=scheduler(0)->scheduled;
 for(size_t i=0;i<LIMIT;i++)if(scheduled[i]){uint64_t wait=scheduled[i]->due>now?scheduled[i]->due-now:0;if(wait<*delay)*delay=(uint32_t)wait;}
 return 0;
}
int im2_timers_poll(size_t *callbacks){
 Scheduler *s=scheduler(1);uint64_t now;if(!s || !callbacks || s->polling || clock_now(&now))return -1;*callbacks=0;
 Timer **scheduled=s->scheduled;
 Timer *batch[LIMIT];size_t n=0;for(size_t i=0;i<LIMIT;i++)if(scheduled[i] && scheduled[i]->due<=now)batch[n++]=im2_objc_retain(scheduled[i]);
 if(!n)return 0;
 void *pool=im2_objc_alloc(im2_objc_class_named("NSAutoreleasePool"),NULL);
 if(!pool || im2_objc_pool_push(pool)){if(pool)im2_objc_release(pool);for(size_t i=0;i<n;i++)im2_objc_release(batch[i]);return -1;}
 s->polling=1;int result=0;
 for(size_t i=0;i<n;i++){Timer *t=batch[i];if(!t->valid || !t->scheduled)continue;
  if(t->repeats){uint64_t wait=t->interval-((now-t->due)%t->interval);if(UINT64_MAX-now<wait){result=-1;break;}t->due=now+wait;}
  if(invoke(t)){result=-1;break;}(*callbacks)++;
 }
 if(im2_objc_pool_drain(pool))result=-1;
 for(size_t i=0;i<n;i++)im2_objc_release(batch[i]);
 s->polling=0;return result;
}
