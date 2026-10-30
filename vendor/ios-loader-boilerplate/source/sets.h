/* SPDX-License-Identifier: MIT. Immutable NSSet subset used by legacy UIKit touch delivery. */
#pragma once
#include "objc_runtime.h"
int im2_sets_init(Im2ObjcClass *root);
void *im2_set_create(void *const *items,size_t count,int autorelease);
