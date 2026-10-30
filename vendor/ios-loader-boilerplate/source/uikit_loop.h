/* SPDX-License-Identifier: MIT. Main-thread timer loop; not complete UIKit event dispatch. */
#pragma once
#include <stdint.h>
typedef struct {
 void *context;
 int (*poll)(void *);
 int (*wait_us)(void *,uint32_t microseconds);
 int (*should_stop)(void *); /* NULL for the process-lifetime Vita loop. */
 uint32_t max_wait_us;
} Im2UIKitLoop;
int im2_uikit_loop_turn(const Im2UIKitLoop *);
int im2_uikit_loop_run(const Im2UIKitLoop *);
