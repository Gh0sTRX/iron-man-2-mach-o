/* SPDX-License-Identifier: MIT. Pump scheduled original selectors, never a guessed draw IMP. */
#include "uikit_loop.h"
#include "timers.h"
static int valid(const Im2UIKitLoop *p){return p && p->poll && p->wait_us && p->max_wait_us && p->max_wait_us<=1000000;}
int im2_uikit_loop_turn(const Im2UIKitLoop *p){
 if(!valid(p) || p->poll(p->context))return -1;
 size_t fired;uint32_t delay;
 if(im2_timers_poll(&fired) || im2_timers_delay(p->max_wait_us,&delay))return -1;
 return delay?p->wait_us(p->context,delay):0;
}
int im2_uikit_loop_run(const Im2UIKitLoop *p){
 if(!valid(p))return -1;
 while(!p->should_stop || !p->should_stop(p->context))if(im2_uikit_loop_turn(p))return -1;
 return 0;
}
