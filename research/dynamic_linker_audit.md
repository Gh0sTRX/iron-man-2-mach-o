# Original Iron Man 2 1.0.0 Mach-O: dynamic linker audit

Source: owner-provided `IronMan2_1.0.0_ios_2.2.1.ipa`, entry `Payload/IronMan2.app/IronMan2`. No proprietary bytes included.

## Reproducible load-command findings
- CPU ARM (12), subtype ARMv6 (6), MH_EXECUTE (2), cryptid=0.
- LC_SYMTAB: 65,500 total nlist entries.
- **LC_DYSYMTAB: iundefsym=65,010; nundefsym=490**. These 490 are the actual dynamic undefined-symbol range.
- Generic nlist scan had counted 1,190 undefined-type records; this is **not** the dynamic import count. Prefer DYSYMTAB for loader scope.
- Indirect symbols: 1,696 entries, offset 5,992,856.
- External relocations: 5,225 entries, offset 5,951,056. All recorded as ARM_RELOC_VANILLA (type=0), extern=1.
- Local relocations: 0 listed in LC_DYSYMTAB. This does **not** prove no rebasing is necessary; inspect section pointers and dyld semantics.
- Actual 490 undefined imports include 77 `_gl*`, 7 `_objc*`, 11 `_Audio*` names.

## Loader development sequence
1. Parse all segment/section file offsets, bounds and VM addresses; build host-side mapped image, preserve page zero.
2. Enumerate DYSYMTAB undefined symbols and external relocations; resolve to a test symbol table; assert bounds, width, PC-relative semantics and expected original values.
3. Inspect indirect symbol pointers and lazy/non-lazy stubs; validate symbol ordinal/library mapping.
4. Generate relocation diagnostics before attempting a `rebase.bin`; compare with MetalSyntax loader source, license and target ABI.
5. Audit ObjC runtime, UIKit lifecycle, GL ES fixed-function calls and AudioUnit independently.
6. Run Vita ARM ELF build only after host checks; do not claim actual gameplay until physical Vita test.

## Open questions
- Can the original ARMv6 instructions execute correctly on Vita ARMv7 with this ABI?
- Can Vita memory placement preserve required addresses?
- Which Objective-C classes/selectors are dynamically referenced beyond the 490 C-symbol imports?
- Which GL ES fixed-function entry points need translation to vitaGL?
- Does the game require additional on-disk resources and iOS runtime behavior during boot?
