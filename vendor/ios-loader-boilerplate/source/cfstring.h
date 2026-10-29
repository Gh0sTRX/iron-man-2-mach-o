/* SPDX-License-Identifier: MIT. Borrowed ARM32 CFString literals, ASCII and UTF-16LE. */
#pragma once
#include "objc_guest.h"
int im2_cfstrings_register(uint32_t address,size_t count,Im2ObjcClass *string_class,
    Im2GuestRead read,void *context,char *error,size_t error_size);
const char *im2_cfstring_utf8(const void *identity,size_t *bytes,uint32_t *utf16_units);
int im2_cfstrings_clear(void); /* loader teardown/test only, never during guest execution */
