/* SPDX-License-Identifier: MIT. Independent CFRunLoop default-mode timer subset.
 * No port/network/perform-block sources are registered or faked. Timers alone
 * never produce kCFRunLoopRunHandledSource (4), even with returnAfterSourceHandled.
 */
#include "runloop.h"
#include "timers.h"
#include "foundation.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
enum {RUN_FINISHED=1,RUN_TIMED_OUT=3};
typedef struct {int running,have_clock;uint64_t previous;} RunState;
void *im2_cf_default_mode;
static Im2RunLoopConfig config;
static pthread_key_t key;
static int ready;
static void fail(const char *message){if(config.failure)config.failure(message);abort();}
int im2_runloop_init(Im2RunLoopConfig value){
 if(ready || !value.now_us || !value.wait_us || !im2_objc_class_named("NSTimer"))return -1;
 if(pthread_key_create(&key,free))return -1;
 im2_cf_default_mode=im2_string_create("kCFRunLoopDefaultMode",0);
 if(!im2_cf_default_mode){pthread_key_delete(key);return -1;}
 config=value;ready=1;return 0;
}
static uint64_t clock_now(RunState *state){
 uint64_t now=config.now_us(config.context);
 if(state->have_clock && now<state->previous)fail("CFRunLoop monotonic clock moved backwards");
 state->have_clock=1;state->previous=now;return now;
}
int32_t im2_cf_runloop_run(void *mode,uint32_t low,uint32_t high,uint32_t return_after_source){
 (void)return_after_source; /* Boolean does not turn timer callbacks into sources. */
 const char *name=ready?im2_string_utf8(mode):NULL;
 if(!name || strcmp(name,"kCFRunLoopDefaultMode"))fail("Unsupported CFRunLoop mode");
 uint64_t bits=(uint64_t)high<<32|low;double seconds;memcpy(&seconds,&bits,8);
 if(!isfinite(seconds) || seconds>31536000.)fail("Unsupported CFRunLoop time interval");
 if(seconds<0)seconds=0;
 RunState *state=pthread_getspecific(key);
 if(!state){state=calloc(1,sizeof(*state));if(!state)fail("CFRunLoop state allocation failed");
  if(pthread_setspecific(key,state)){free(state);fail("CFRunLoop TLS registration failed");}}
 if(state->running)fail("Nested CFRunLoop is not supported by timer-only backend");
 uint64_t now=clock_now(state),span=(uint64_t)ceil(seconds*1000000.);
 if(UINT64_MAX-now<span)fail("CFRunLoop timeout overflow");
 uint64_t deadline=now+span;state->running=1;
 int32_t result=RUN_FINISHED;
 if(im2_timers_scheduled())for(;;){
  pthread_testcancel();
  size_t fired;if(im2_timers_poll(&fired))fail("CFRunLoop timer dispatch failed");
  now=clock_now(state);
  if(now>=deadline){result=RUN_TIMED_OUT;break;}
  if(!im2_timers_scheduled())break;
  uint64_t left=deadline-now;uint32_t delay,cap=left>10000?10000:(uint32_t)left;
  if(im2_timers_delay(cap,&delay))fail("CFRunLoop timer deadline failed");
  if(delay && config.wait_us(config.context,delay))fail("CFRunLoop wait failed");
 }
 state->running=0;return result;
}
