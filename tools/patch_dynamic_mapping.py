#!/usr/bin/env python3
"""Second-stage patch for MetalSyntax macho.c AFTER tools/patch_loader_v2.py.

Builds a contiguous __TEXT/__DATA image at the *actual* allocated base.
This is a source-level experiment; executable memory permission is unverified.
"""
from pathlib import Path
import argparse

def one(s,old,new,label):
    if old in s:
        if s.count(old)!=1: raise ValueError(label+" ambiguous")
        return s.replace(old,new,1)
    if new in s:return s
    raise ValueError(label+" source mismatch")

def patch(src):
    s=one(src,"static uintptr_t text_real;","static uintptr_t text_real;\nstatic uint32_t macho_slide;\nstatic SceUID image_blk = -1;","slide declaration")
    s=s.replace("MACHO_SLIDE + ","macho_slide + ").replace("+= MACHO_SLIDE;","+= macho_slide;")
    if "MACHO_SLIDE" in s:raise ValueError("Unconverted fixed slide")
    before='''    // Memory block allocations
    SceUID text_blk = sceKernelAllocMemBlockForVM("macho_text", text_size);
    if (text_blk < 0)
        fatal_error("sceKernelAllocMemBlockForVM(text) = 0x%08X", (unsigned)text_blk);
    void *text_ptr = NULL;
    sceKernelGetMemBlockBase(text_blk, &text_ptr);
    text_real = (uintptr_t)text_ptr;

    SceUID data_blk = sceKernelAllocMemBlockForVM("macho_data", data_size);
    if (data_blk < 0)
        fatal_error("sceKernelAllocMemBlockForVM(data) = 0x%08X", (unsigned)data_blk);
    void *data_ptr = NULL;
    sceKernelGetMemBlockBase(data_blk, &data_ptr);
'''
    after='''    // Keep original Mach-O virtual offsets in a single allocation.
    if (data_vm < text_vm || text_vm + text_size > data_vm ||
        data_vm > UINT32_MAX - data_size)
        fatal_error("Invalid Mach-O segment layout");
    uint32_t image_span = data_vm + data_size - text_vm;
    image_blk = sceKernelAllocMemBlockForVM("macho_image", image_span);
    if (image_blk < 0)
        fatal_error("Mach-O image allocation failed: 0x%08X", (unsigned)image_blk);
    void *text_ptr = NULL;
    if (sceKernelGetMemBlockBase(image_blk, &text_ptr) < 0 || !text_ptr)
        fatal_error("Cannot obtain Mach-O image base");
    text_real = (uintptr_t)text_ptr;
    if (text_real < text_vm || text_real > UINT32_MAX - image_span)
        fatal_error("Mach-O image outside 32-bit address range");
    macho_slide = (uint32_t)(text_real - text_vm);
    void *data_ptr = (void *)(uintptr_t)(macho_slide + data_vm);
    l_info("Image base=%p, span=0x%X, slide=0x%08X",
           text_ptr, (unsigned)image_span, (unsigned)macho_slide);
'''
    s=one(s,before,after,"contiguous allocation")
    guard='''    if ((uintptr_t)text_ptr != (uintptr_t)(macho_slide + text_vm) ||
        (uintptr_t)data_ptr != (uintptr_t)(macho_slide + data_vm)) {
        l_error("Unsafe Mach-O mapping: text actual=%p expected=%p; data actual=%p expected=%p",
                text_ptr, (void *)(uintptr_t)(macho_slide + text_vm),
                data_ptr, (void *)(uintptr_t)(macho_slide + data_vm));
        return -1;
    }
'''
    s=one(s,guard,"","old fixed mapping guard")
    s=one(s,'''                    if (sym_idx & INDIRECT_SYMBOL_LOCAL) {
                        slots[i] += macho_slide;
''','''                    if (sym_idx & INDIRECT_SYMBOL_LOCAL) {
                        // Local slots are included in RBS1: apply slide once there.
''',"local pointer double relocation")
    s=one(s,'    l_info("Mach-O loaded successfully.");','''    // CAUTION: RW VM mapping is not proof of executable pages.
    l_info("Mach-O mapped; executable permission NOT VERIFIED.");''',"execution caveat")
    return s

if __name__=="__main__":
    a=argparse.ArgumentParser();a.add_argument("source",type=Path);a.add_argument("--output",type=Path);v=a.parse_args()
    (v.output or v.source).write_text(patch(v.source.read_text()))
