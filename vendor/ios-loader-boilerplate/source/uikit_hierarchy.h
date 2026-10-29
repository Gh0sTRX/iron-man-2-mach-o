/* SPDX-License-Identifier: MIT
 * Bootstrapping a measured subset of the legacy iOS UIKit hierarchy.
 * No replacement of Apple's rendering, touch events, or lifecycle is claimed.
 */
#pragma once
#include "objc_runtime.h"

/* iOS ARM32 CGFloat=float, CGPoint=(float,float), CGRect=(CGPoint,CGSize). */
typedef struct {float x,y,width,height;} Im2Rect32;

/* Registers verified guest superclass identities, plus limited view state.
 * Registration must happen before parsing __objc_classlist. */
int im2_uikit_hierarchy_init(Im2ObjcClass *nsobject,void (*failure)(const char *));
