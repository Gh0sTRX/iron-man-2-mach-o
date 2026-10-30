/* SPDX-License-Identifier: MIT
 * Partial UIKit UIScreen. The original owner's Mach-O selects precisely
 * +[UIScreen mainScreen] and -[UIScreen bounds] in its external selector
 * references. CGRect returns through objc_msgSend_stret (hidden ARM32 r0).
 */
#include "uikit_screen.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#if UINTPTR_MAX==UINT32_MAX
_Static_assert(sizeof(Im2Rect32)==16u,"ARM32 CGRect must occupy four floats");
#endif

typedef struct {
    Im2ObjcClass *isa;
    Im2Rect32 bounds;
} Screen;

static Im2ObjcClass *screen_class;
static Screen *main_screen;

static uintptr_t screen_main(void *cls,const char *selector) {
    (void)selector;
    return cls==screen_class?(uintptr_t)main_screen:0u;
}

/* Explicit stret signature: r0 -> out, r1 -> receiver, r2 -> selector.
 * Never return CGRect from this function as a C aggregate: that would
 * depend on the host compiler's ABI and could clobber guest stack slots. */
static void screen_bounds(Im2Rect32 *out,void *receiver,const char *selector) {
    (void)selector;
    if(out && receiver==main_screen)
        *out=main_screen->bounds;
}

/* Not an Objective-C method: prevents treating missing classes as views. */
int im2_uikit_screen_init(Im2ObjcClass *root,Im2ScreenGeometry geometry) {
    if(screen_class || main_screen || !root ||
        !isfinite(geometry.width_points) || !isfinite(geometry.height_points) ||
        geometry.width_points<=0 || geometry.height_points<=0 ||
        geometry.width_points>4096 || geometry.height_points>4096)
        return -1;
    size_t checkpoint=im2_objc_registration_checkpoint();
    const Im2MethodSpec methods[]={
        {"bounds","{CGRect={CGPoint=ff}{CGSize=ff}}@:",(Im2Imp)screen_bounds,sizeof(Im2Rect32)}
    };
    const Im2MethodSpec class_methods[]={
        {"mainScreen","@@:",(Im2Imp)screen_main,0}
    };
    screen_class=im2_objc_register_class("UIScreen",root,sizeof(Screen),
        methods,sizeof(methods)/sizeof(*methods),
        class_methods,sizeof(class_methods)/sizeof(*class_methods));
    if(!screen_class)return -1;
    main_screen=im2_objc_alloc(screen_class,NULL);
    if(!main_screen) {
        (void)im2_objc_registration_rollback(checkpoint);
        screen_class=NULL;return -1;
    }
    main_screen->bounds=(Im2Rect32){0.f,0.f,
        geometry.width_points,geometry.height_points};
    return 0;
}
int im2_uikit_screen_shutdown(void) {
    if(!main_screen || im2_objc_retain_count(main_screen)!=1)return -1;
    Screen *ending=main_screen;
    main_screen=NULL;
    return im2_objc_release(ending);
}
