/* SPDX-License-Identifier: MIT. Bounded reader for rebased 32-bit ObjC2 class lists. */
#pragma once
#include "objc_runtime.h"
typedef const uint8_t *(*Im2GuestRead)(void *,uint32_t address,size_t bytes,int executable);
/* Startup only. All metadata is validated before classes are registered; registration
 * failure rolls back the batch. External parents must already have real host classes.
 * Class identities, method addresses, and guest object layouts remain original. */
int im2_objc_register_guest_list(uint32_t classlist,size_t count,Im2GuestRead read,
    void *context,char *error,size_t error_size);
