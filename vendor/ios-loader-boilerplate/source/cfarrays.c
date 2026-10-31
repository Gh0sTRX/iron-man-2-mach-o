/* SPDX-License-Identifier: MIT
 * Measured owner path: default allocator, capacity=0, NULL callbacks. Values are
 * borrowed opaque pointers: never retain, release, inspect or dereference them.
 * The array itself participates in the existing CFRetain/CFRelease runtime.
 */
#include "cfarrays.h"
#include "imports.h"
#include "log.h"
#include <stdlib.h>
#define CF_ARRAY_LIMIT 65536u

typedef struct { Im2ObjcClass *isa; const void **items; size_t count,capacity; } RawArray;
static Im2ObjcClass *array_class;
static const void *const default_allocator=NULL;
static RawArray *checked(const void *value) {
    if(!array_class || im2_objc_object_class(value)!=array_class || !im2_objc_retain_count(value))
        fatal_error("CFArray operation requires a live supported CF array");
    return (RawArray *)value;
}
static void clear(RawArray *a) { a->count=0; } /* No value lifetime callbacks. */
static void destroy(void *value) {
    RawArray *a=value;free(a->items);a->items=NULL;a->count=a->capacity=0;
}
static void *create(const void *allocator,int32_t capacity,const void *callbacks) {
    if(allocator || capacity || callbacks)
        fatal_error("CFArrayCreateMutable supports default allocator, capacity zero and NULL callbacks only");
    if(!array_class)fatal_error("CFArray runtime is not initialized");
    return im2_objc_alloc(array_class,destroy); /* Create rule: +1, no pool. */
}
static void append(void *value,const void *item) {
    RawArray *a=checked(value);
    if(a->count==CF_ARRAY_LIMIT)fatal_error("CFArray supported element limit exceeded");
    if(a->count==a->capacity) {
        size_t capacity=a->capacity?a->capacity*2:8;
        const void **items=realloc(a->items,capacity*sizeof(*items));
        if(!items)fatal_error("CFArray storage allocation failed");
        a->items=items;a->capacity=capacity;
    }
    a->items[a->count++]=item;
}
static int32_t count(const void *value) { return (int32_t)checked(value)->count; }
static const void *at(const void *value,int32_t index) {
    RawArray *a=checked(value);
    if(index<0 || (size_t)index>=a->count)fatal_error("CFArray index outside bounds");
    return a->items[index]; /* Get rule: borrowed. */
}
static void remove_all(void *value) { clear(checked(value)); }
static uintptr_t objc_count(void *self,const char *selector) { (void)selector;return (uintptr_t)count(self); }
static uintptr_t objc_at(void *self,const char *selector,uint32_t index) {
    (void)selector;if(index>INT32_MAX)fatal_error("CF array object index outside bounds");
    return (uintptr_t)at(self,(int32_t)index);
}
int im2_cfarrays_init(Im2ObjcClass *root) {
    if(!root || array_class)return -1;
    const Im2MethodSpec methods[]={
        {"count","I@:",(Im2Imp)objc_count,0},
        {"objectAtIndex:","@@:I",(Im2Imp)objc_at,0}
    };
    /* Private concrete class, not a replacement NSArray/toll-free bridge. */
    array_class=im2_objc_register_class("__IM2CFMutableArray",root,sizeof(RawArray),methods,2,NULL,0);
    return array_class?0:-1;
}
const import_entry cfarray_imports[]={
    F(CFArrayCreateMutable,create), F(CFArrayAppendValue,append), F(CFArrayGetCount,count),
    F(CFArrayGetValueAtIndex,at), F(CFArrayRemoveAllValues,remove_all),
    D(kCFAllocatorDefault,&default_allocator),
};
const int cfarray_imports_count=sizeof(cfarray_imports)/sizeof(cfarray_imports[0]);
