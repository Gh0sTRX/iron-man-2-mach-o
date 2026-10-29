/* SPDX-License-Identifier: MIT
 * Asset-free owner-style UIApplicationMain launch stage. The production guest
 * method path is simulated by an explicit ARM32 register-call callback.
 */
#define _POSIX_C_SOURCE 200809L
#include "uikit_bootstrap.h"
#include "uikit_application.h"
#include "uikit_hierarchy.h"
#include "foundation.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int callbacks;
static void *expected_imp;
static void *app_at_launch;
static uintptr_t root_alloc(void *klass,const char *selector) {
    (void)selector;
    return (uintptr_t)im2_objc_alloc(klass,NULL);
}
static uintptr_t root_init(void *receiver,const char *selector) {
    (void)selector;return (uintptr_t)receiver;
}
static void guest_launch(void *receiver,const char *selector,void *app) {
    (void)receiver;(void)selector;(void)app;
    /* Should never be reached directly: this method is a guest IMP. */
    assert(!"native dispatcher must go through guest ARM register bridge");
}
static uint32_t fake_guest_call(void *imp,uint32_t self,
    uint32_t selector,uint32_t arg0,uint32_t arg1) {
    assert(imp==expected_imp);
    assert(self && selector && arg0 && !arg1);
    assert(selector==(uint32_t)(uintptr_t)im2_objc_sel_register("applicationDidFinishLaunching:"));
    callbacks++;
    (void)guest_launch;
    return 0;
}
static void *send0(void *receiver,const char *selector) {
    Im2ObjcTarget target;
    assert(!im2_objc_lookup_target(receiver,selector,NULL,&target));
    assert(!target.guest && !target.stret_size);
    return (void *)((uintptr_t(*)(void *,const char *))target.imp)(receiver,selector);
}
int main(void) {
    const Im2MethodSpec cls[]={{"alloc","@@:",(Im2Imp)root_alloc,0}};
    const Im2MethodSpec ims[]={{"init","@@:",(Im2Imp)root_init,0}};
    Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,
        sizeof(Im2ObjcObject),ims,1,cls,1);
    assert(root);
    Im2FoundationConfig config={"/tmp",NULL,NULL,NULL};
    assert(!im2_foundation_init(root,&config));
    assert(!im2_uikit_hierarchy_init(root,NULL));
    assert(!im2_uikit_application_init(root,NULL));
    Im2ObjcClass *appcls=im2_objc_class_named("UIApplication");
    assert(appcls);
    void *app=send0(appcls,"sharedApplication");
    assert(app);
    void *name=im2_string_create("AppController",0);
    void *bad=im2_string_create("UnknownDelegate",0);
    void *principal=im2_string_create("UIApplication",0);
    assert(name && bad && principal);
    const Im2MethodSpec guest_ims[]={
        {"applicationDidFinishLaunching:","v@:@",(Im2Imp)guest_launch,0}
    };
    expected_imp=(void *)guest_launch;
    Im2ObjcClass *guest=im2_objc_register_foreign_class("AppController",
        (void *)(uintptr_t)0x24000000,(void *)(uintptr_t)0x24000020,
        root,16,guest_ims,1,NULL,0);
    assert(guest);
    char arg0[]="IronMan2";
    char *argv[]={arg0,NULL};
    assert(im2_uikit_launch_once(1,argv,NULL,bad,fake_guest_call)==IM2_UIKIT_BOOT_INVALID);
    assert(im2_uikit_launch_once(1,argv,name,bad,fake_guest_call)==IM2_UIKIT_BOOT_INVALID);
    assert(im2_uikit_launch_once(1,argv,NULL,name,NULL)==IM2_UIKIT_BOOT_INVALID);
    assert(!send0(app,"delegate") && callbacks==0);
    /* The original code supplies nil principal. Explicit UIApplication also works. */
    assert(im2_uikit_launch_once(1,argv,principal,name,fake_guest_call)==IM2_UIKIT_BOOT_NEEDS_RUN_LOOP);
    assert(callbacks==1);
    app_at_launch=send0(app,"delegate");
    assert(app_at_launch && im2_objc_object_class(app_at_launch)==guest);
    assert(im2_uikit_launch_once(1,argv,NULL,name,fake_guest_call)==IM2_UIKIT_BOOT_INVALID);
    assert(callbacks==1);
    assert(!im2_uikit_launch_release_delegate());
    assert(!send0(app,"delegate"));
    assert(im2_uikit_launch_release_delegate());

    assert(!im2_objc_release(name));
    assert(!im2_objc_release(bad));
    assert(!im2_objc_release(principal));
    assert(!im2_uikit_application_shutdown());
    puts("PASS: original-style AppController delegate class validation, real alloc/init resolution, guest callback and no fake UIKit loop success");
    return 0;
}
