/* SPDX-License-Identifier: MIT. Original UIKit touch delivery, no guessed game engine entry points. */
#pragma once
#include "objc_runtime.h"
#include "uikit_bootstrap.h"
#include "uikit_hierarchy.h"
enum {IM2_TOUCH_LIMIT=6,IM2_TOUCH_BEGAN=0,IM2_TOUCH_MOVED=1,IM2_TOUCH_STATIONARY=2,IM2_TOUCH_ENDED=3,IM2_TOUCH_CANCELLED=4};
typedef struct {int id;float x,y;} Im2TouchSample; /* Native 960x544 display pixels. */
int im2_touches_init(Im2ObjcClass *root,Im2UIKitGuestCall);
int im2_touches_frame(const Im2TouchSample *,size_t count,uint64_t time_us,void *window,size_t *callbacks);
int im2_touches_cancel(uint64_t time_us,size_t *callbacks);
int im2_touches_shutdown(void);
/* Inverse of the shared landscape-right FBO presentation. False for letterbox areas. */
int im2_touches_project(float screen_x,float screen_y,Im2Point32 *window_point);
