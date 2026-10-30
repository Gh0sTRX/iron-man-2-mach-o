/* SPDX-License-Identifier: MIT
 * Original ARM32 UIKit bootstrap stage. No fabricated main event loop.
 * All delegate messages resolve via the real registered ObjC method tables.
 */
#include "uikit_bootstrap.h"
#include "foundation.h"
#include "objc_runtime.h"
#include <stdint.h>
#include <string.h>

static void *launch_delegate;
static void *launch_pool;
static int launching,launched;

static int string_name(void *object,const char **out) {
    *out=NULL;
    if(!object)return -1;
    const char *name=im2_string_utf8(object);
    if(!name || !*name || strlen(name)!=im2_string_byte_length(object))
        return -1;
    *out=name;
    return 0;
}
static int native_or_guest_send(void *receiver,const char *selector,
    void *arg,int has_arg,Im2UIKitGuestCall guest_bridge,void **out) {
    Im2ObjcTarget target;
    const char *registered=im2_objc_sel_register(selector);
    if(!registered ||
       im2_objc_lookup_target(receiver,registered,NULL,&target) ||
       !target.imp || target.stret_size)
        return -1;
    uintptr_t result=0;
    if(target.guest) {
        if(!guest_bridge)return -1;
        result=guest_bridge((void *)target.imp,
            (uint32_t)(uintptr_t)receiver,(uint32_t)(uintptr_t)registered,
            (uint32_t)(uintptr_t)(has_arg?arg:NULL),0);
    } else if(has_arg) {
        /* The legacy delegate callback and setDelegate: are void methods;
         * calling them through a pointer-returning prototype is undefined C. */
        if(!strcmp(selector,"setDelegate:") ||
           !strcmp(selector,"applicationDidFinishLaunching:"))
            ((void(*)(void *,const char *,void *))target.imp)(
                receiver,registered,arg);
        else
            result=((uintptr_t(*)(void *,const char *,void *))target.imp)(
                receiver,registered,arg);
    } else {
        result=((uintptr_t(*)(void *,const char *))target.imp)(
            receiver,registered);
    }
    if(out)*out=(void *)result;
    return 0;
}
int im2_uikit_launch_once(int argc,char *const *argv,
    void *principal_name,void *delegate_name,
    Im2UIKitGuestCall guest_bridge) {
    if(launching || launched || launch_pool || argc<0 || (argc && !argv))
        return IM2_UIKIT_BOOT_INVALID;
    Im2ObjcClass *app_class=im2_objc_class_named("UIApplication");
    if(!app_class)return IM2_UIKIT_BOOT_INVALID;
    /* The original main uses nil. Other subclasses need separate ABI support. */
    if(principal_name) {
        const char *name;
        if(string_name(principal_name,&name) || strcmp(name,"UIApplication"))
            return IM2_UIKIT_BOOT_INVALID;
    }
    const char *delegate_class_name;
    if(string_name(delegate_name,&delegate_class_name))
        return IM2_UIKIT_BOOT_INVALID;
    Im2ObjcClass *cls=im2_objc_class_named(delegate_class_name);
    Im2ObjcClass *pool_class=im2_objc_class_named("NSAutoreleasePool");
    if(!cls || cls==app_class || !im2_objc_class_metadata(cls) || !pool_class)
        return IM2_UIKIT_BOOT_INVALID;
    /* Do not call a guest function without an explicit ARM32 call bridge. */
    if(!guest_bridge)return IM2_UIKIT_BOOT_INVALID;
    void *app=NULL,*delegate=NULL,*initialized=NULL;
    launching=1;
    launch_pool=im2_objc_alloc(pool_class,NULL);
    if(!launch_pool)goto fail;
    if(im2_objc_pool_push(launch_pool)) {
        (void)im2_objc_release(launch_pool);launch_pool=NULL;goto fail;
    }
    if(native_or_guest_send(app_class,"sharedApplication",NULL,0,
        guest_bridge,&app) || !app)
        goto fail;
    if(native_or_guest_send(cls,"alloc",NULL,0,
        guest_bridge,&delegate) || !delegate)
        goto fail;
    if(native_or_guest_send(delegate,"init",NULL,0,
        guest_bridge,&initialized) || initialized!=delegate)
        goto fail;
    if(native_or_guest_send(app,"setDelegate:",delegate,1,
        guest_bridge,NULL))
        goto fail;
    Im2ObjcTarget callback;
    if(im2_objc_lookup_target(delegate,"applicationDidFinishLaunching:",
        NULL,&callback) || !callback.imp || callback.stret_size)
        goto fail;
    /* iOS 2/3 callback receives UIApplication itself (not a notification).
     * The owner's selector encoding is v12@0:4@8. */
    if(native_or_guest_send(delegate,"applicationDidFinishLaunching:",
        app,1,guest_bridge,NULL))
        goto fail;
    launch_delegate=delegate;
    launched=1;launching=0;
    return IM2_UIKIT_BOOT_NEEDS_RUN_LOOP;
fail:
    if(app)(void)native_or_guest_send(app,"setDelegate:",NULL,1,guest_bridge,NULL);
    if(launch_pool) {
        void *pool=launch_pool;launch_pool=NULL;
        (void)im2_objc_pool_drain(pool);
    }
    if(delegate && im2_objc_retain_count(delegate)>0)
        (void)im2_objc_release(delegate);
    launching=0;
    return IM2_UIKIT_BOOT_INVALID;
}
int im2_uikit_launch_release_delegate(void) {
    if(!launched || !launch_delegate)return -1;
    Im2ObjcClass *app_class=im2_objc_class_named("UIApplication");
    void *app=NULL;
    if(!app_class || native_or_guest_send(app_class,"sharedApplication",
        NULL,0,NULL,&app) || !app)
        return -1;
    if(native_or_guest_send(app,"setDelegate:",NULL,1,NULL,NULL))
        return -1;
    /* Drain Cocoa temporaries created by the guest callback before releasing
     * the launch-owned delegate. Runtime pool ordering is thread-local. */
    if(!launch_pool)return -1;
    void *pool=launch_pool;
    launch_pool=NULL;
    if(im2_objc_pool_drain(pool))return -1;
    if(im2_objc_retain_count(launch_delegate)>0 &&
       im2_objc_release(launch_delegate))return -1;
    launch_delegate=NULL;launched=0;
    return 0;
}
