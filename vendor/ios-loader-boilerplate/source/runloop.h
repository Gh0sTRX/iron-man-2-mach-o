/* SPDX-License-Identifier: MIT. Default-mode timer-only CFRunLoop subset. */
#pragma once
#include <stdint.h>
typedef struct {
 void *context;
 uint64_t (*now_us)(void *);
 int (*wait_us)(void *,uint32_t);
 void (*failure)(const char *);
} Im2RunLoopConfig;
int im2_runloop_init(Im2RunLoopConfig);
extern void *im2_cf_default_mode;
/* Measured owner ARMv6 ABI: mode=r0, double bits=r1/r2, Boolean=r3.
 * Do not declare a native double (VFP) or add AAPCS double padding. */
int32_t im2_cf_runloop_run(void *mode,uint32_t seconds_low,uint32_t seconds_high,
                         uint32_t return_after_source);
