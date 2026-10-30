/* SPDX-License-Identifier: MIT
 * Deliberately partial UIKit state. No fake UIApplicationMain return, event loop,
 * touch handling or actual screen orientation/render effects are synthesized.
 */
#include "uikit_application.h"
#include "uikit_hierarchy.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    Im2ObjcClass *isa;
    void *delegate;    /* UIKit delegate is non-owning in the legacy runtime. */
    void *key_window;  /* Strongly held until another window becomes key. */
    uint32_t orientation;
    uint32_t status_bar_hidden;
    uint32_t idle_disabled;
} Application;

static Im2ObjcClass *application_class;
static Application *application;
static void (*failure)(const char *);

static void fail(const char *reason) {
    if(failure)failure(reason);
    abort(); /* An error callback returning cannot signal fake UIKit success. */
}
static int is_app(const void *object) {
    return object==application && application_class &&
        im2_objc_object_class(object)==application_class;
}
static void app_destroy(void *object) {
    Application *app=object;
    if(app->key_window) {
        im2_objc_release(app->key_window);
        app->key_window=NULL;
    }
    app->delegate=NULL;
    if(object==application)application=NULL;
}
static int activate_window(void *window) {
    Im2ObjcClass *cls=im2_objc_class_named("UIWindow");
    if(!application || !cls || !window)return -1;
    Im2ObjcClass *current=im2_objc_object_class(window);
    int is_window=0;
    for(size_t depth=0;current && depth<32;depth++) {
        if(current==cls){is_window=1;break;}
        const Im2ObjcClass *meta=im2_objc_class_metadata(current);
        if(!meta)break;
        current=meta->superclass;
    }
    if(!is_window)return -1;
    void *held=im2_objc_retain(window);
    if(!held)return -1;
    void *previous=application->key_window;
    application->key_window=held;
    if(previous)im2_objc_release(previous);
    return 0;
}
static uintptr_t shared_application(void *cls,const char *selector) {
    (void)selector;
    if(cls!=application_class || !application)fail("UIApplication singleton unavailable");
    return (uintptr_t)application;
}
static uintptr_t get_delegate(void *receiver,const char *selector) {
    (void)selector;
    if(!is_app(receiver))fail("UIApplication delegate receiver invalid");
    return (uintptr_t)application->delegate;
}
static void set_delegate(void *receiver,const char *selector,void *delegate) {
    (void)selector;
    if(!is_app(receiver))fail("UIApplication setDelegate receiver invalid");
    if(delegate && !im2_objc_object_class(delegate))
        fail("UIApplication delegate is not a registered Objective-C instance");
    application->delegate=delegate;
}
static uintptr_t key_window(void *receiver,const char *selector) {
    (void)selector;
    if(!is_app(receiver))fail("UIApplication keyWindow receiver invalid");
    return (uintptr_t)application->key_window;
}
static void set_status_hidden(void *receiver,const char *selector,uint32_t hidden) {
    (void)selector;
    if(!is_app(receiver))fail("UIApplication setStatusBarHidden receiver invalid");
    application->status_bar_hidden=(uint8_t)hidden!=0;
}
static uint32_t is_status_hidden(void *receiver,const char *selector) {
    (void)selector;
    if(!is_app(receiver))fail("UIApplication isStatusBarHidden receiver invalid");
    return application->status_bar_hidden;
}
static void set_orientation(void *receiver,const char *selector,uint32_t orientation) {
    (void)selector;
    if(!is_app(receiver))fail("UIApplication setStatusBarOrientation receiver invalid");
    if(orientation<1 || orientation>4)
        fail("Unsupported UIApplication status bar orientation value");
    application->orientation=orientation;
}
static void set_orientation_animated(void *receiver,const char *selector,
    uint32_t orientation,uint32_t animated) {
    (void)animated;
    set_orientation(receiver,selector,orientation);
}
static uint32_t get_orientation(void *receiver,const char *selector) {
    (void)selector;
    if(!is_app(receiver))fail("UIApplication statusBarOrientation receiver invalid");
    return application->orientation;
}
static void set_idle(void *receiver,const char *selector,uint32_t value){(void)selector;if(!is_app(receiver))fail("Invalid UIApplication idle timer receiver");application->idle_disabled=(uint8_t)value!=0;}
static uint32_t get_idle(void *receiver,const char *selector){(void)selector;if(!is_app(receiver))fail("Invalid UIApplication idle timer receiver");return application->idle_disabled;}
#define METHOD(s,t,f) {s,t,(Im2Imp)(f),0}
int im2_uikit_application_init(Im2ObjcClass *root,void (*on_failure)(const char *)) {
    if(application_class || !root ||
        !im2_objc_class_named("UIResponder") || !im2_objc_class_named("UIWindow"))
        return -1;
    failure=on_failure;
    const Im2MethodSpec instance[]={
        METHOD("delegate","@@:",get_delegate),
        METHOD("setDelegate:","v@:@",set_delegate),
        METHOD("keyWindow","@@:",key_window),
        METHOD("setIdleTimerDisabled:","v@:c",set_idle),
        METHOD("isIdleTimerDisabled","c@:",get_idle),
        METHOD("setStatusBarHidden:","v@:c",set_status_hidden),
        METHOD("isStatusBarHidden","c@:",is_status_hidden),
        METHOD("setStatusBarOrientation:","v@:i",set_orientation),
        METHOD("setStatusBarOrientation:animated:","v@:ic",set_orientation_animated),
        METHOD("statusBarOrientation","i@:",get_orientation)
    };
    const Im2MethodSpec class_methods[]={
        METHOD("sharedApplication","@@:",shared_application)
    };
    size_t checkpoint=im2_objc_registration_checkpoint();
    application_class=im2_objc_register_class("UIApplication",
        im2_objc_class_named("UIResponder"),sizeof(Application),
        instance,sizeof(instance)/sizeof(*instance),
        class_methods,sizeof(class_methods)/sizeof(*class_methods));
    if(!application_class)return -1;
    application=im2_objc_alloc(application_class,app_destroy);
    if(!application || im2_uikit_bind_window_activation(activate_window)) {
        if(application)im2_objc_release(application);
        application=NULL;application_class=NULL;
        (void)im2_objc_registration_rollback(checkpoint);
        return -1;
    }
    application->orientation=1; /* UIInterfaceOrientationPortrait. */
    /* Original owner IPA's Info.plist explicitly declares UIStatusBarHidden=true. */
    application->status_bar_hidden=1;
    return 0;
}

int im2_uikit_application_shutdown(void) {
    if(!application || im2_objc_retain_count(application)!=1)return -1;
    return im2_objc_release(application);
}

void *im2_uikit_key_window(void){return application?application->key_window:NULL;}

int im2_uikit_idle_timer_disabled(void){return application?application->idle_disabled!=0:0;}
