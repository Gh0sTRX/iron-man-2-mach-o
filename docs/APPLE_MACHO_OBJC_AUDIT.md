# Apple Mach-O / Objective-C original image audit — 2026-10-29

Evidence: locally inspected the user-provided, decrypted `IronMan2` binary inside original iOS IPA. Game binary not uploaded to GitHub.

## Actual Objective-C 32-bit Mach-O sections

| Section | Byte length | 32-bit entries | Nonzero | Distinct raw values |
|---|---:|---:|---:|---:|
| `__objc_selrefs` | 1928 | 482 | 482 | 455 |
| `__objc_classrefs` | 972 | 243 | 25 | 24 |
| `__objc_superrefs` | 96 | 24 | 24 | 24 |
| `__objc_classlist` | 100 | 25 | 25 | 25 |
| `__objc_protolist` | 64 | 16 | 16 | 16 |

Other sections: `__objc_data` 960 bytes, `__objc_const` 18,160 bytes, `__cfstring` 2,640 bytes.

**Critical distinction:** 218 zero-filled classref slots do not establish 218 distinct missing classes. They may be dyld-bound external class pointers. Resolve slot ownership through `LC_DYSYMTAB` indirect symbol indices and undefined symbol names; classify `_OBJC_CLASS_$_...` as data imports. Never bind these to a generic 256-byte placeholder: Objective-C class objects require valid ISA, superclass, cache and method structures or a correctly isolated HLE object model.

## Apple format references
- Apple Mach-O overview: https://developer.apple.com/library/archive/documentation/Performance/Conceptual/CodeFootprint/Articles/MachOOverview.html
- Apple open-source Mach-O layout: https://github.com/apple-oss-distributions/xnu/blob/main/EXTERNAL_HEADERS/mach-o/loader.h
- Apple dyld relocation logic: https://github.com/apple-oss-distributions/dyld/blob/main/common/MachOAnalyzer.cpp
- Apple symbolication explanation: https://developer.apple.com/videos/play/wwdc2021/10211/

## Changes in this batch
- Validated `LC_ENCRYPTION_INFO` 20-byte minimum command and encrypted region file bounds.
- Removed dead local-indirect rebase fallback; mandatory RBS1 already rebases local slots once.
- Added read-only original ObjC section census tool `tools/audit_objc_sections.py`.
- Added synthetic parser negative tests `tests/test_objc_sections.py` and wired them into CI.

## Remaining blockers
1. Identify each zero classref slot's associated undefined symbol and correct data representation.
2. Distinguish old iOS ObjC2 class/metaclass references, selector strings and method dispatch ABI.
3. Verify ARMv6 `objc_msgSend` forwarding (including no-arg and struct returns) with actual callsite disassembly, not only synthetic checks.
4. Build the full loader using VitaSDK and prove kubridge executable-memory/cache behavior on real hardware.
5. Finish missing iOS imports and subsystem behavior before VPK.
