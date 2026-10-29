#!/usr/bin/env python3
"""Final compile-shape corrections after loader_v2 and dynamic_mapping patches.
Only source transformations: still requires VitaSDK build and Vita3K execution.
"""
from pathlib import Path
import argparse
def patch(s):
    # Original Mach-O section struct uses reserved1, not r1.
    if "s->r1" in s:
        s=s.replace("s->r1", "s->reserved1")
    if "s->type" in s:raise ValueError("Invalid section field s->type remains")
    # Log actual addresses only after runtime slide is calculated.
    start='''    l_info("__TEXT vm: 0x%08X (size 0x%08X) -> real 0x%08X",
           (unsigned)text_vm, (unsigned)text_size, (unsigned)(macho_slide + text_vm));
    l_info("__DATA vm: 0x%08X (size 0x%08X) -> real 0x%08X",
           (unsigned)data_vm, (unsigned)data_size, (unsigned)(macho_slide + data_vm));

'''
    if start in s:
        s=s.replace(start,"",1)
        needle='''    macho_slide = (uint32_t)(text_real - text_vm);
'''
        if needle not in s:raise ValueError("Runtime slide calculation missing")
        s=s.replace(needle,needle+start,1)
    if "s->r1" in s:raise ValueError("Reserved field mismatch")
    return s
if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("source",type=Path);p.add_argument("--output",type=Path);a=p.parse_args()
    (a.output or a.source).write_text(patch(a.source.read_text()))
