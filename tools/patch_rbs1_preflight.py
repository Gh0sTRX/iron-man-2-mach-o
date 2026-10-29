#!/usr/bin/env python3
"""Harden RBS1 application in MetalSyntax macho.c (after dynamic mapping patch).
Fails closed on truncated, duplicate, misaligned or out-of-image entries.
"""
from pathlib import Path
import argparse
START='static void apply_rebase(const char *rebase_path) {'
END='\nint macho_load(const char *bin_path, const char *rebase_path) {'
def patch(source):
    if 'RBS1 strict preflight' in source:return source
    if source.count(START)!=1 or source.count(END)!=1:raise ValueError('Unsupported upstream layout')
    start=source.index(START);end=source.index(END,start)
    replacement=r'''/* RBS1 strict preflight: no writes until the complete list is validated. */
static int rbs1_cmp(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}
static int apply_rebase(const char *rebase_path) {
    FILE *f = fopen(rebase_path, "rb");
    if (!f) { l_error("Required RBS1 missing: %s", rebase_path); return -1; }
    uint32_t magic = 0, count = 0;
    if (fread(&magic, 4, 1, f) != 1 || magic != 0x31534252u ||
        fread(&count, 4, 1, f) != 1 || count == 0 || count > 1000000u) {
        fclose(f); l_error("Invalid RBS1 header/count"); return -1;
    }
    uint32_t *entries = malloc((size_t)count * sizeof(*entries));
    if (!entries) { fclose(f); return -1; }
    int good = fread(entries, sizeof(*entries), count) == count &&
               fgetc(f) == EOF && !ferror(f);
    fclose(f);
    if (!good) { free(entries); l_error("Truncated or oversized RBS1"); return -1; }
    qsort(entries, count, sizeof(*entries), rbs1_cmp);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t va = entries[i];
        if ((va & 3u) || (i && va == entries[i-1])) { good = 0; break; }
        int in_data = va >= data_vm && va - data_vm <= data_size - 4u;
        int in_text = va >= text_vm && va - text_vm <= text_size - 4u;
        if (!in_data && !in_text) { good = 0; break; }
        uint32_t value = in_data ? *(uint32_t *)(uintptr_t)(macho_slide + va)
                                 : *(uint32_t *)(text_buf + va - text_vm);
        /* Thumb function pointers may use bit 0; retain it when checking range. */
        uint32_t target = value & ~1u;
        if (target < text_vm || target >= data_vm + data_size) {
            good = 0; break;
        }
        if (value > UINT32_MAX - macho_slide) { good = 0; break; }
    }
    if (!good) { free(entries); l_error("RBS1 preflight rejected; image not modified"); return -1; }
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t va = entries[i];
        uint32_t *slot = va >= data_vm
          ? (uint32_t *)(uintptr_t)(macho_slide + va)
          : (uint32_t *)(text_buf + va - text_vm);
        *slot += macho_slide;
    }
    free(entries);
    l_info("RBS1 applied: %u entries", (unsigned)count);
    return 0;
}
'''
    out=source[:start]+replacement+source[end:]
    old='''    if (rebase_path)
        apply_rebase(rebase_path);'''
    new='''    if (!rebase_path || apply_rebase(rebase_path) < 0)
        fatal_error("Mach-O relocation preflight failed");'''
    if old not in out:raise ValueError('Missing apply_rebase call site')
    return out.replace(old,new,1)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
    (a.output or a.source).write_text(patch(a.source.read_text()))
