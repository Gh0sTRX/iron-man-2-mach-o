# Loader fixes — verified against Iron Man 2 iOS 1.0.0

## Blocking bug 1: LC_SEGMENT header offset

In upstream `source/macho.c`, `segment_command` omits `cmd` and `cmdsize` yet code casts the start of LC_SEGMENT to it. Original executable segment names then decode as `\\x01`, not `__TEXT` or `__DATA`, so loader cannot find its segments.

Fix struct:
```c
typedef struct {
    uint32_t cmd, cmdsize;
    char segname[16];
    uint32_t vmaddr, vmsize, fileoff, filesize, maxprot, initprot, nsects, flags;
} segment_command;
```
Or use a pointer to `d + off + 8` with original struct, but then section offset logic must be checked.

## Blocking risk 2: constructor double-slide

Original IPA has `__mod_init_func` at `0x4B901C`, size `1188` bytes: **297 nonzero function pointers**. First raw values are `0x2588, 0x2624, 0x26E0, 0x277C`.

Upstream `macho_run_inits` sets `mod_init_addr = MACHO_SLIDE + section.addr` then calls `fn = funcs[i] + MACHO_SLIDE`. If `apply_rebase` already relocated these slots, this becomes a double slide. Proposed robust fix: inspect whether each pointer lies in original image VA range vs slid VA range, and apply the slide only if necessary. Also verify `__mod_init_func` slots are actually present in RBS1 before making assumptions.

## Additional critical runtime blocker: actual Vita memory placement

Upstream allocates `text_blk` and `data_blk` and retrieves `text_ptr`/`data_ptr`, but then copies to `MACHO_SLIDE + text_vm` and writes `MACHO_SLIDE + data_vm` without showing a mapping to those addresses. This must be verified with the actual kubridge allocation/mapping setup. **Do not run an unvalidated fixed-address copy on hardware.**

## Evidence / reproducer

Run `python3 tools/loader_preflight.py IronMan2_1.0.0_ios_2.2.1.ipa`. This reads the original binary in memory; no proprietary files are committed.

## Status

Host-level structure tests only. No claim of successful Vita execution. Existing Iron-man-2-os repo unchanged.
