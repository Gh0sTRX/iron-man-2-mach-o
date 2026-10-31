/* SPDX-License-Identifier: MIT
 * CoreGraphics ARM32 value operations only; no CGContext/image/view rendering.
 * CGFloat is float, and aggregate results use the caller's hidden output pointer.
 */
#include <math.h>
#include "imports.h"
typedef struct { float x,y; } CGPoint32;
typedef struct { float x,y,width,height; } CGRect32;
typedef struct { float a,b,c,d,tx,ty; } CGAffine32;
_Static_assert(sizeof(CGPoint32)==8 && sizeof(CGRect32)==16 && sizeof(CGAffine32)==24,
               "iOS ARM32 CoreGraphics value layout required");
static const CGPoint32 cg_point_zero={0,0};
static const CGRect32 cg_rect_zero={0,0,0,0};
SOFTFP static CGAffine32 cg_rotation(float radians) {
    float c=cosf(radians),s=sinf(radians);
    return (CGAffine32){c,s,-s,c,0,0};
}
SOFTFP static CGAffine32 cg_scale(float x,float y) {
    return (CGAffine32){x,0,0,y,0,0};
}
SOFTFP static CGRect32 cg_rect_offset(CGRect32 r,float dx,float dy) {
    r.x+=dx;r.y+=dy;return r;
}
const import_entry cg_geometry_imports[] = {
 D(CGPointZero,&cg_point_zero), D(CGRectZero,&cg_rect_zero),
 F(CGAffineTransformMakeRotation,cg_rotation), F(CGAffineTransformMakeScale,cg_scale),
 F(CGRectOffset,cg_rect_offset),
};
const int cg_geometry_imports_count=sizeof(cg_geometry_imports)/sizeof(cg_geometry_imports[0]);
