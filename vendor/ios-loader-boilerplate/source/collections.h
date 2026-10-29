/* SPDX-License-Identifier: MIT. Partial Foundation collections, startup registry. */
#pragma once
#include "objc_runtime.h"
int im2_collections_init(Im2ObjcClass *root,void (*failure)(const char *));
void *im2_array_create(void *const *items,size_t count,int mutable,int autorelease);
void *im2_dictionary_create(void *const *values,void *const *keys,size_t count,int mutable,int autorelease);

/* Native unsigned long/pointers are 32-bit under VitaSDK, matching the guest ABI. */
typedef struct {
    uintptr_t state;
    void **items;
    uintptr_t *mutations;
    uintptr_t extra[5];
} Im2FastEnumerationState;
#if UINTPTR_MAX==UINT32_MAX
_Static_assert(sizeof(Im2FastEnumerationState)==32,"iOS ARM32 NSFastEnumerationState");
#endif
