#!/usr/bin/env python3
"""Apply fail-closed structural patches to MetalSyntax macho.c."""
from pathlib import Path
import argparse

def replace_once(src, old, new, name):
    if old in src:
        return src.replace(old, new, 1)
    if new in src:
        return src
    raise ValueError("Upstream source mismatch: "+name)

def patch(src):
    src=replace_once(src,"typedef struct {\n    char segname[16];","typedef struct {\n    uint32_t cmd, cmdsize;\n    char segname[16];","LC_SEGMENT header")
    src=replace_once(src,"if (s->type == S_NON_LAZY_SYMBOL_POINTERS || s->type == S_LAZY_SYMBOL_POINTERS)",
      "if ((s->flags & 0xFFu) == S_NON_LAZY_SYMBOL_POINTERS || (s->flags & 0xFFu) == S_LAZY_SYMBOL_POINTERS)","section flags")
    src=replace_once(src,"uintptr_t fn = funcs[i] + MACHO_SLIDE;",'''uintptr_t fn = funcs[i];
            if (fn >= text_vm && fn < data_vm + data_size)
                fn += MACHO_SLIDE;
            else if (fn < MACHO_SLIDE + text_vm || fn >= MACHO_SLIDE + data_vm + data_size) {
                l_error("Constructor outside loaded image: 0x%08X", (unsigned)fn);
                continue;
            }''',"constructor double slide")
    if "Unsafe Mach-O mapping:" not in src:
        src=replace_once(src,"    // Copy segment contents\n",'''    if ((uintptr_t)text_ptr != (uintptr_t)(MACHO_SLIDE + text_vm) ||
        (uintptr_t)data_ptr != (uintptr_t)(MACHO_SLIDE + data_vm)) {
        l_error("Unsafe Mach-O mapping: text actual=%p expected=%p; data actual=%p expected=%p",
                text_ptr, (void *)(uintptr_t)(MACHO_SLIDE + text_vm),
                data_ptr, (void *)(uintptr_t)(MACHO_SLIDE + data_vm));
        return -1;
    }
    // Copy segment contents
''',"fixed mapping guard")
    return src

if __name__=="__main__":
    a=argparse.ArgumentParser();a.add_argument("source",type=Path);a.add_argument("--output",type=Path);args=a.parse_args()
    out=args.output or args.source;out.write_text(patch(args.source.read_text()));print("Patched",out)
