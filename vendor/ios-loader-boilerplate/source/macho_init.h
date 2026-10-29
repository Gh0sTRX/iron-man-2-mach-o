/* SPDX-License-Identifier: MIT. Validate every constructor before executing any. */
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef void (*Im2InitCall)(void *context,uint32_t function);
int im2_run_initializers(const uint32_t *functions,size_t count,uint32_t text_base,
    uint32_t text_size,Im2InitCall call,void *context);
