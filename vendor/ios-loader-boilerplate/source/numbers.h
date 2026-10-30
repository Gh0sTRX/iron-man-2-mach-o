/* SPDX-License-Identifier: MIT. Integral NSNumber subset with explicit Darwin return ABI. */
#pragma once
#include "objc_runtime.h"
int im2_numbers_init(Im2ObjcClass *root);
void *im2_number_bool(int value,int autorelease);
void *im2_number_signed(int64_t value,int autorelease);
void *im2_number_unsigned(uint64_t value,int autorelease);
int im2_number_bool_value(const void *object,int *value);
