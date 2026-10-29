/* SPDX-License-Identifier: MIT
 * Real registered native UIKit methods, superclass sizes, stateful view
 * ownership and fail-closed unsupported lazy view loading.
 */
#define _POSIX_C_SOURCE 200809L
#include "uikit_hierarchy.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static Im2Imp method(void *receiver,const char *selector) {
    Im2ObjcTarget target;
    assert(!im2_objc_lookup_target(receiver,selector,NULL,&target));
    assert(!target.guest && !target.stret_size);
    return target.imp;
}
static void invoke_dealloc(void *receiver) {
    const char *sel="dealloc";
    ((void(*)(void *,const char *))method(receiver,sel))(receiver,sel);
}
static void fail(const char *reason) {(void)reason;_exit(73);}
static void test_failure(void *controller) {
    pid_t pid=fork();assert(pid>=0);
    if(!pid) {
        const char *sel="view";
        (void)((uintptr_t(*)(void *,const char *))method(controller,sel))(controller,sel);
        _exit(74);
    }
    int status;assert(waitpid(pid,&status,0)==pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status)==73);
}
static void set_hidden(void *view,int value) {
    const char *sel="setHidden:";
    ((void(*)(void *,const char *,int))method(view,sel))(view,sel,value);
}
static uint32_t hidden(void *view) {
    const char *sel="isHidden";
    return ((uint32_t(*)(void *,const char *))method(view,sel))(view,sel);
}
static void set_tag(void *view,int value) {
    const char *sel="setTag:";
    ((void(*)(void *,const char *,int))method(view,sel))(view,sel,value);
}
static int tag(void *view) {
    const char *sel="tag";
    return ((int(*)(void *,const char *))method(view,sel))(view,sel);
}
static void *init_frame(void *view,Im2Rect32 r) {
    const char *sel="initWithFrame:";
    return (void *)((uintptr_t(*)(void *,const char *,Im2Rect32))method(view,sel))(view,sel,r);
}
static void set_frame(void *view,Im2Rect32 r) {
    const char *sel="setFrame:";
    ((void(*)(void *,const char *,Im2Rect32))method(view,sel))(view,sel,r);
}
static void set_view(void *controller,void *view) {
    const char *sel="setView:";
    ((void(*)(void *,const char *,void *))method(controller,sel))(controller,sel,view);
}
static void *get_view(void *controller) {
    const char *sel="view";
    return (void *)((uintptr_t(*)(void *,const char *))method(controller,sel))(controller,sel);
}
static void assert_class(const char *name,const char *parent,uint32_t arm_size) {
    Im2ObjcClass *cls=im2_objc_class_named(name);
    Im2ObjcClass *p=im2_objc_class_named(parent);
    const Im2ObjcClass *meta=im2_objc_class_metadata(cls);
    assert(cls && p && meta && meta->superclass==p);
    assert(meta->data->ro->instance_size==arm_size);
    assert(im2_objc_class_metadata(meta->isa));
}
int main(void) {
    Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);
    assert(root);
    assert(!im2_uikit_hierarchy_init(root,fail));
    assert(im2_uikit_hierarchy_init(root,fail));
    assert_class("UIView","UIResponder",36);
    assert_class("UIControl","UIView",36);
    assert_class("UIButton","UIControl",126);
    assert_class("UIImageView","UIView",40);
    assert_class("UIViewController","UIResponder",92);
    assert_class("UINavigationController","UIViewController",168);
    assert_class("UIWindow","UIView",36);

    Im2ObjcClass *image=im2_objc_class_named("UIImageView");
    Im2ObjcClass *controller_class=im2_objc_class_named("UINavigationController");
    void *v=im2_objc_alloc(image,invoke_dealloc);
    void *controller=im2_objc_alloc(controller_class,invoke_dealloc);
    assert(v && controller);
    Im2Rect32 frame={12,21,480,320},actual={0};
    assert(im2_uikit_view_frame(v,&actual));
    assert(init_frame(v,frame)==v);
    assert(!im2_uikit_view_frame(v,&actual));
    assert(actual.x==12 && actual.y==21 && actual.width==480 && actual.height==320);
    set_frame(v,(Im2Rect32){0,0,240,160});
    assert(!im2_uikit_view_frame(v,&actual) && actual.width==240);
    set_hidden(v,3);assert(hidden(v)==1);
    set_hidden(v,0);assert(hidden(v)==0);
    set_tag(v,108);assert(tag(v)==108);
    test_failure(controller); /* Lazy loadView not yet implemented. */
    assert(im2_objc_retain_count(v)==1);
    set_view(controller,v);assert(get_view(controller)==v);
    assert(im2_objc_retain_count(v)==2);
    assert(!im2_objc_release(v)); /* Controller still owns view. */
    assert(!im2_objc_release(controller)); /* Releases view too. */
    assert(im2_objc_retain_count(v)==0);

    /* The guest EAGLView requires its exact UIView superclass instance size. */
    Im2ObjcClass *eagl=im2_objc_register_class("EAGLView",
        im2_objc_class_named("UIView"),128,NULL,0,NULL,0);
    assert(eagl);
    void *guest_like=im2_objc_alloc(eagl,invoke_dealloc);
    assert(guest_like);
    assert(init_frame(guest_like,frame)==guest_like);
    assert(!im2_uikit_view_frame(guest_like,&actual) && actual.width==480);
    assert(!im2_objc_release(guest_like));
    puts("PASS: measured UIKit class tree, inherited UIView state, CGFloat32 CGRect, view ownership and explicit lazy-load failure");
    return 0;
}
