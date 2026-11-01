/* SPDX-License-Identifier: MIT. Per-thread default-mode NSTimer subset. */
#pragma once
#include "objc_runtime.h"
#include "uikit_bootstrap.h"
typedef struct {
 uint64_t (*now_us)(void *context);
 void *context;
 Im2UIKitGuestCall guest;
 void (*failure)(const char *);
} Im2TimerConfig;
int im2_timers_init(Im2ObjcClass *root,Im2TimerConfig);
/* APIs act on the current pthread queue; each timer is restricted to its owner.
 * Thread exit invalidates its scheduled timers without firing callbacks. */
int im2_timers_poll(size_t *callbacks);
int im2_timers_delay(uint32_t cap_us,uint32_t *delay_us);
size_t im2_timers_scheduled(void);
int im2_timers_cancel_all(void);
