/* SPDX-License-Identifier: MIT. Checked ARM32 contiguous mapping arithmetic. */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* VitaSDK sceKernelAllocMemBlockForVM has a documented 16 MiB limit. */
#define IM2_VM_MAX_SPAN (16u * 1024u * 1024u)
typedef struct { uint32_t text_size, data_size, span; } Im2MachoLayout;
static inline int im2_macho_layout(uint32_t tv, uint32_t ts, uint32_t tf,
    uint32_t tn, uint32_t dv, uint32_t ds, uint32_t df, uint32_t dn,
    size_t file_size, Im2MachoLayout *out) {
    if (!out || !ts || !ds || (tv & 4095u) || (dv & 4095u) ||
        tn > ts || dn > ds || (uint64_t)tf + tn > file_size ||
        (uint64_t)df + dn > file_size) return -1;
    const uint64_t text = ((uint64_t)ts + 4095u) & ~UINT64_C(4095);
    const uint64_t data = ((uint64_t)ds + 4095u) & ~UINT64_C(4095);
    const uint64_t end = (uint64_t)dv + data;
    if (text > UINT32_MAX || data > UINT32_MAX || dv < tv ||
        (uint64_t)tv + text > dv || end > UINT32_MAX ||
        end - tv > IM2_VM_MAX_SPAN) return -1;
    *out = (Im2MachoLayout){(uint32_t)text, (uint32_t)data, (uint32_t)(end-tv)};
    return 0;
}
