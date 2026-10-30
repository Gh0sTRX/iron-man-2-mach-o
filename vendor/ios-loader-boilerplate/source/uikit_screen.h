/* SPDX-License-Identifier: MIT
 * UIScreen subset for the original non-Retina iPhone ARMv6 Iron Man 2 build.
 * Screen points and CGRect host IMP ABI are explicit; PS Vita display
 * pixel scaling and orientation rendering are not implied.
 */
#pragma once
#include "objc_runtime.h"
#include "uikit_hierarchy.h"

typedef struct {
    float width_points;
    float height_points;
} Im2ScreenGeometry;

/* The owner's iPhone 2.2.1 IPA uses a 320x480 point canvas.
 * Configure once before guest class registration. */
int im2_uikit_screen_init(Im2ObjcClass *root, Im2ScreenGeometry geometry);

/* Controlled teardown only: not part of UIKit ABI. */
int im2_uikit_screen_shutdown(void);
