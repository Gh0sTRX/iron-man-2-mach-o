#!/usr/bin/env python3
"""Apply two targeted source fixes to MetalSyntax loader; refuse unexpected versions."""
import argparse
from pathlib import Path
OLD_SEG='''typedef struct {
    char segname[16];
    uint32_t vmaddr, vmsize, fileoff, filesize, maxprot, initprot, nsects, flags;
} segment_command;'''
NEW_SEG='''typedef struct {
    uint32_t cmd, cmdsize; // LC_SEGMENT header precedes segname
    char segname[16];
    uint32_t vmaddr, vmsize, fileoff, filesize, maxprot, initprot, nsects, flags;
} segment_command;'''
OLD_INIT='''            uintptr_t fn = funcs[i] + MACHO_SLIDE;
            call_guest((void *)fn, 0, 0, 0, 0);'''
NEW_INIT='''            // __mod_init_func may already have been rebased by apply_rebase().
            uintptr_t fn = funcs[i];
            uintptr_t image_lo = text_vm;
            uintptr_t image_hi = data_vm + data_size;
            if (fn >= image_lo && fn < image_hi) {
                fn += MACHO_SLIDE;
            } else if (fn < MACHO_SLIDE + image_lo ||
                       fn >= MACHO_SLIDE + image_hi) {
                l_error("Invalid __mod_init_func target: 0x%08X", (unsigned)fn);
                continue;
            }
            call_guest((void *)fn, 0, 0, 0, 0);'''


def patch(src):
    for old,new,label in [(OLD_SEG,NEW_SEG,'LC_SEGMENT header'),(OLD_INIT,NEW_INIT,'constructor double-slide')]:
        if old in src: src=src.replace(old,new,1)
        elif new in src: print('Already patched:',label)
        else: raise RuntimeError('Source mismatch for '+label)
    return src

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
    original=a.source.read_text();fixed=patch(original)
    dest=a.output or a.source
    dest.write_text(fixed)
    print('Wrote',dest)
