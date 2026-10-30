/* SPDX-License-Identifier: MIT
 * Bootstrapping a measured subset of the legacy iOS UIKit hierarchy.
 * No replacement of Apple's rendering, touch events, or lifecycle is claimed.
 */
#pragma once
#include "objc_runtime.h"

/* Darwin passes CGRect arguments in core registers/stack, not VFP HFA registers. */
#if defined(__arm__)
#define IM2_UIKIT_PCS __attribute__((pcs("aapcs")))
#else
#define IM2_UIKIT_PCS
#endif

/* iOS ARM32 CGFloat=float, CGPoint=(float,float), CGRect=(CGPoint,CGSize). */
typedef struct {float x,y,width,height;} Im2Rect32;

/* Registers verified guest superclass identities, plus limited view state.
 * Registration must happen before parsing __objc_classlist. */
int im2_uikit_hierarchy_init(Im2ObjcClass *nsobject,void (*failure)(const char *));

/* Internal observation for tests and eventual renderer; does not mimic CGRect
 * return ABI. Returns -1 until a frame was explicitly initialized. */
int im2_uikit_view_frame(const void *receiver,Im2Rect32 *out);
/* Internal dirty flags pending a real Vita layout and rendering pipeline. */
int im2_uikit_view_invalidation(const void *receiver,int *needs_layout,int *needs_display);
/* Registers one application-owned UIWindow activation hook. The window class
 * deliberately refuses makeKeyAndVisible before an application is initialized. */
int im2_uikit_bind_window_activation(int (*activate)(void *window));
/* Lazy backing layer allocation delegated to the CAEAGLLayer module.
 * The UIView sidecar takes ownership of the returned +1 object. */
int im2_uikit_bind_layer_factory(void *(*create)(void *view));


/* Main-thread translation-only touch routing. Affine transforms are unsupported. */
typedef struct {float x,y;} Im2Point32;
void *im2_uikit_hit_test(void *window,Im2Point32 point);
void *im2_uikit_view_parent(void *view);
int im2_uikit_view_origin(void *view,void *window,Im2Point32 *origin);
int im2_uikit_view_visible(void *view,void *window);

int im2_uikit_view_accepts_multiple(void *view);
