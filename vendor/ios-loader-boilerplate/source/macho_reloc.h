/* SPDX-License-Identifier: MIT. Classic ARM32 MH_EXECUTE external relocation planner. */
#pragma once
#include <stddef.h>
#include <stdint.h>
/* r_address is the original linked VM address for this non-PIE executable path.
 * map returns staging/RW word storage; resolve returns an ABI-bridged address.
 * A resolver may return nonzero to report an unsupported symbol. */
typedef uint8_t *(*Im2RelocMap)(void *,uint32_t);
typedef int (*Im2RelocResolve)(void *,uint32_t,uint32_t *);
int im2_apply_external_relocs(const void *records,size_t count,uint32_t symbol_count,
    Im2RelocMap map,Im2RelocResolve resolve,void *context,char *error,size_t error_size);
