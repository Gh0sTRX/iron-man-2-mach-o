/* SPDX-License-Identifier: MIT
 * New independent runtime core; ABI layouts researched against objc4-437.
 * It implements explicit host classes only, not Apple's complete Objective-C runtime. */
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct im2_objc_class Im2ObjcClass;
typedef void (*Im2Imp)(void);
typedef struct { const char *selector, *types; Im2Imp imp; size_t stret_size; } Im2MethodSpec;
typedef struct {
    uint32_t flags, instance_start, instance_size;
#if UINTPTR_MAX > UINT32_MAX
    uint32_t reserved;
#endif
    const void *ivar_layout;
    const char *name;
    const void *methods, *protocols, *ivars, *weak_layout, *properties;
} Im2ClassRO;
typedef struct {
    uint32_t flags, version;
    const Im2ClassRO *ro;
    const void *methods, *properties, *protocols;
    Im2ObjcClass *first_subclass, *next_sibling;
} Im2ClassRW;
struct im2_objc_class {
    Im2ObjcClass *isa, *superclass;
    void *cache, *vtable;
    Im2ClassRW *data;
};
typedef struct { Im2ObjcClass *isa; } Im2ObjcObject;
/* Registration is a startup-only operation. No blank framework classes are created. */
Im2ObjcClass *im2_objc_register_class(const char *,Im2ObjcClass *,size_t,
    const Im2MethodSpec *,size_t,const Im2MethodSpec *,size_t);
Im2ObjcClass *im2_objc_class_named(const char *);
int im2_objc_is_class(const void *);
const char *im2_objc_sel_register(const char *);
Im2Imp im2_objc_lookup(const void *,const char *,size_t *stret_size);
void *im2_objc_alloc(Im2ObjcClass *,void (*destroy)(void *));
void *im2_objc_retain(void *);
int im2_objc_release(void *);
void *im2_objc_autorelease(void *);
size_t im2_objc_retain_count(const void *);
/* Thread-local nested pools; releasing an out-of-order pool is rejected. */
int im2_objc_pool_push(void *pool_object);
int im2_objc_pool_drain(void *pool_object);
