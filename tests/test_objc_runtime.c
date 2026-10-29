/* Behavioral tests for the exact core linked into the Vita loader. No guest/game data. */
#include "objc_runtime.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
static int destroyed;
static int checked_dealloc;
static void destroy(void *p) { (void)p; __sync_fetch_and_add(&destroyed,1); }
static void root_method(void) {}
static void dealloc_check(void *p) {
    assert(im2_objc_retain_count(p)==0);
    assert(im2_objc_lookup(p,"instance",NULL)==root_method);
    assert(!im2_objc_retain(p));
    assert(im2_objc_release(p)==-1);
    checked_dealloc++;
}
static void child_method(void) {}
static void class_method(void) {}
static void struct_method(void) {}
static Im2ObjcClass *root,*child;
static void *worker(void *p) {
    void *parent=p;
    void *pool=im2_objc_alloc(root,NULL);
    void *object=im2_objc_alloc(child,destroy);
    assert(im2_objc_autorelease(object)==NULL); /* main thread pool is invisible */
    assert(im2_objc_pool_drain(parent)==-1);
    assert(im2_objc_pool_push(pool)==0);
    assert(im2_objc_autorelease(object)==object);
    assert(im2_objc_pool_drain(pool)==0);
    return NULL;
}
int main(void) {
    const Im2MethodSpec rm[]={{"instance","v@:",root_method,0}};
    const Im2MethodSpec cm[]={{"factory","@@:",class_method,0}};
    root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),rm,1,cm,1);
    assert(root && root->isa->isa==root->isa && root->isa->superclass==root && !root->superclass);
    const Im2MethodSpec methods[]={{"override","v@:",child_method,0},{"size","{Size=ff}@:",struct_method,8}};
    child=im2_objc_register_class("NSString",root,sizeof(Im2ObjcObject)+16,methods,2,NULL,0);
    assert(child && child->isa->isa==root->isa && child->isa->superclass==root->isa);
    assert(root->data->first_subclass==child);
    assert(((const void *const *)child->data->methods)[0]==child->data->ro->methods);
    assert(((const void *const *)child->data->methods)[1]==NULL);
    assert(child->superclass==root && child->data->ro->instance_start==sizeof(Im2ObjcObject));
    assert(!im2_objc_register_class("NSString",root,64,NULL,0,NULL,0));
    assert(!im2_objc_register_class("OtherRoot",NULL,64,NULL,0,NULL,0));
    assert(!im2_objc_class_named("UIView")); /* no guessed UIKit success */
    assert(!im2_objc_alloc(child->isa,NULL));
    assert(im2_objc_class_named("NSString")==child);
    assert(im2_objc_sel_register("length")==im2_objc_sel_register("length"));
    assert(!im2_objc_sel_register(NULL));
    void *object=im2_objc_alloc(child,destroy); size_t stret=99;
    assert(((Im2ObjcObject *)object)->isa==child);
    assert(im2_objc_lookup(object,"instance",&stret)==root_method && stret==0);
    assert(im2_objc_lookup(object,"size",&stret)==struct_method && stret==8);
    assert(im2_objc_lookup(child,"factory",NULL)==class_method);
    assert(!im2_objc_lookup(object,"factory",NULL));
    assert(!im2_objc_lookup((void *)(uintptr_t)0x1234,"length",NULL));
    assert(!im2_objc_lookup(object,"missing",NULL));
    assert(!im2_objc_lookup(NULL,"instance",NULL));
    assert(im2_objc_retain_count(object)==1 && im2_objc_retain(object)==object);
    assert(im2_objc_retain_count(object)==2);
    assert(!im2_objc_release(object) && !destroyed);
    void *outer=im2_objc_alloc(root,NULL),*inner=im2_objc_alloc(root,NULL);
    assert(!im2_objc_pool_push(outer)); assert(im2_objc_pool_push(outer)==-1);
    assert(im2_objc_autorelease(object)==object);
    assert(!im2_objc_pool_push(inner));
    assert(im2_objc_pool_drain(outer)==-1);
    for(int i=0;i<400;i++) assert(im2_objc_autorelease(im2_objc_alloc(child,destroy)));
    /* The old 256-slot NSString ring could not preserve these lifetimes. */
    assert(!im2_objc_pool_drain(inner));assert(destroyed==400);
    assert(im2_objc_retain_count(object)==1);
    pthread_t thread;assert(!pthread_create(&thread,NULL,worker,outer));assert(!pthread_join(thread,NULL));
    assert(destroyed==401 && im2_objc_retain_count(object)==1);
    assert(!im2_objc_pool_drain(outer));assert(destroyed==402);
    assert(!im2_objc_retain_count(object));
    assert(!im2_objc_release(root));assert(im2_objc_retain(root)==root);
    void *dying=im2_objc_alloc(child,dealloc_check);
    assert(!im2_objc_release(dying) && checked_dealloc==1);
    puts("PASS: class/metaclass layout, selectors, inherited/class/stret lookup, ownership, nested and thread-local pools");
}
