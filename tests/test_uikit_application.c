/* SPDX-License-Identifier: MIT
 * Exercises native UIApplication state, key UIWindow retention, orientation,
 * delegate ownership and deliberate absence of UIApplicationMain execution.
 */
#define _POSIX_C_SOURCE 200809L
#include "uikit_application.h"
#include "uikit_hierarchy.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static Im2Imp imp(void *receiver,const char *selector) {
    Im2ObjcTarget target;
    assert(!im2_objc_lookup_target(receiver,selector,NULL,&target));
    assert(!target.guest && !target.stret_size);
    return target.imp;
}
static void *send0(void *receiver,const char *selector) {
    return (void *)((uintptr_t(*)(void *,const char *))imp(receiver,selector))(receiver,selector);
}
static void call0(void *receiver,const char *selector) {
    ((void(*)(void *,const char *))imp(receiver,selector))(receiver,selector);
}
static void call1(void *receiver,const char *selector,uint32_t value) {
    ((void(*)(void *,const char *,uint32_t))imp(receiver,selector))(receiver,selector,value);
}
static uint32_t get_uint(void *receiver,const char *selector) {
    return ((uint32_t(*)(void *,const char *))imp(receiver,selector))(receiver,selector);
}
static void set_delegate(void *receiver,void *delegate) {
    const char *sel="setDelegate:";
    ((void(*)(void *,const char *,void *))imp(receiver,sel))(receiver,sel,delegate);
}
static void dealloc(void *receiver) {
    call0(receiver,"dealloc");
}
static void failure(const char *reason) {(void)reason;_exit(73);}
static void reject_bad_orientation(void *application) {
    pid_t pid=fork();assert(pid>=0);
    if(!pid){call1(application,"setStatusBarOrientation:",99);_exit(74);}
    int st;assert(waitpid(pid,&st,0)==pid);
    assert(WIFEXITED(st) && WEXITSTATUS(st)==73);
}
int main(void) {
    Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);
    assert(root);
    assert(!im2_uikit_hierarchy_init(root,failure));
    assert(!im2_uikit_application_init(root,failure));
    assert(im2_uikit_application_init(root,failure));

    Im2ObjcClass *appcls=im2_objc_class_named("UIApplication");
    Im2ObjcClass *windowcls=im2_objc_class_named("UIWindow");
    assert(appcls && windowcls);
    assert(im2_objc_class_metadata(appcls)->superclass==im2_objc_class_named("UIResponder"));
    void *app=send0(appcls,"sharedApplication");
    assert(app && app==send0(appcls,"sharedApplication"));
    assert(!send0(app,"delegate"));
    assert(!send0(app,"keyWindow"));
    assert(get_uint(app,"statusBarOrientation")==1);
    assert(get_uint(app,"isStatusBarHidden")==1); /* Owner Info.plist hides it. */

    void *delegate=im2_objc_alloc(root,NULL);
    assert(delegate);
    set_delegate(app,delegate);
    assert(send0(app,"delegate")==delegate);
    assert(im2_objc_retain_count(delegate)==1); /* non-owning */
    set_delegate(app,NULL);
    assert(!send0(app,"delegate"));
    assert(!im2_objc_release(delegate));

    call1(app,"setStatusBarHidden:",3);
    assert(get_uint(app,"isStatusBarHidden")==1);
    call1(app,"setStatusBarHidden:",0);
    assert(!get_uint(app,"isStatusBarHidden"));
    call1(app,"setStatusBarOrientation:",3);
    assert(get_uint(app,"statusBarOrientation")==3);
    const char *animated="setStatusBarOrientation:animated:";
    ((void(*)(void *,const char *,uint32_t,uint32_t))imp(app,animated))(app,animated,4,1);
    assert(get_uint(app,"statusBarOrientation")==4);
    reject_bad_orientation(app);

    void *window=im2_objc_alloc(windowcls,dealloc);
    assert(window);
    assert(get_uint(window,"isHidden")==1);
    assert(im2_objc_retain_count(window)==1);
    call0(window,"makeKeyAndVisible");
    assert(send0(app,"keyWindow")==window);
    assert(get_uint(window,"isHidden")==0);
    assert(im2_objc_retain_count(window)==2);
    call0(window,"makeKeyAndVisible"); /* re-activate same window, no extra retain */
    assert(im2_objc_retain_count(window)==2);
    assert(!im2_objc_release(window));
    assert(im2_objc_retain_count(window)==1);

    assert(!im2_uikit_application_shutdown());
    assert(im2_objc_retain_count(window)==0);
    assert(im2_uikit_application_shutdown());
    puts("PASS: UIApplication singleton, delegate ownership, orientation state and UIWindow activation/retention");
    return 0;
}
