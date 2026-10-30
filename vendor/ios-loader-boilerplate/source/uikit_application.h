/* SPDX-License-Identifier: MIT
 * Partial, stateful legacy UIApplication API for original iOS Mach-O.
 * This does not implement UIApplicationMain, an event loop, or UIKit rendering.
 */
#pragma once
#include "objc_runtime.h"

/* Registers the UIApplication singleton and binds UIWindow activation.
 * Must run after im2_uikit_hierarchy_init(). */
int im2_uikit_application_init(Im2ObjcClass *root,void (*failure)(const char *));

/* Controlled process shutdown only; rejects teardown with outstanding retains. */
int im2_uikit_application_shutdown(void);

/* Borrowed current window; main-thread event delivery only. */
void *im2_uikit_key_window(void);

int im2_uikit_idle_timer_disabled(void);
