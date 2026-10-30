/* SPDX-License-Identifier: MIT
 * Host-ABI-independent verification of the native ARM32 UIScreen CGRect
 * hidden-return-pointer IMP and owner 320x480 point canvas.
 */
#include "uikit_screen.h"
#include "objc_runtime.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static Im2ObjcTarget method(void *obj,const char *selector) {
    Im2ObjcTarget t={0};
    assert(!im2_objc_lookup_target(obj,selector,NULL,&t));
    assert(!t.guest && t.imp);
    return t;
}
static void *screen_singleton(void *screen_class) {
    Im2ObjcTarget t=method(screen_class,"mainScreen");
    assert(t.stret_size==0);
    return (void *)((uintptr_t (*)(void *,const char *))t.imp)(
        screen_class,"mainScreen");
}
static void screen_bounds(void *screen,Im2Rect32 *out) {
    Im2ObjcTarget t=method(screen,"bounds");
    assert(t.stret_size==sizeof(Im2Rect32));
    /* C callback matches the 32-bit objc_msgSend_stret hidden-return ABI.
     * Never call an aggregate-returning IMP as an ordinary pointer method. */
    ((void (*)(Im2Rect32 *,void *,const char *))t.imp)(out,screen,"bounds");
}
int main(void) {
    Im2ObjcClass *root=im2_objc_register_class("NSObject",
        NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);
    assert(root);
    assert(im2_uikit_screen_init(root,(Im2ScreenGeometry){0,480}));
    assert(im2_uikit_screen_init(root,(Im2ScreenGeometry){320,NAN}));
    assert(im2_uikit_screen_init(root,(Im2ScreenGeometry){320,4097}));
    assert(!im2_objc_class_named("UIScreen"));
    assert(!im2_uikit_screen_init(root,(Im2ScreenGeometry){320,480}));
    assert(im2_uikit_screen_init(root,(Im2ScreenGeometry){320,480}));
    Im2ObjcClass *cls=im2_objc_class_named("UIScreen");
    assert(cls);
    const Im2ObjcClass *meta=im2_objc_class_metadata(cls);
    assert(meta && meta->superclass==root);
    assert(im2_objc_class_metadata(meta->isa)); /* Legitimate class/metaclass. */
    void *screen=screen_singleton(cls);
    assert(screen && screen==screen_singleton(cls));
    assert(im2_objc_object_class(screen)==cls);
    Im2Rect32 rect={-1,-1,-1,-1};
    screen_bounds(screen,&rect);
    assert(rect.x==0.f && rect.y==0.f &&
           rect.width==320.f && rect.height==480.f);
    assert(sizeof(rect)==16);
    assert(im2_objc_retain_count(screen)==1);
    assert(!im2_uikit_screen_shutdown());
    assert(!screen_singleton(cls));
    assert(im2_uikit_screen_shutdown());
    puts("PASS: UIScreen singleton, original 320x480 point geometry, CGRect stret=16, invalid sizes, and ownership");
    return 0;
}
