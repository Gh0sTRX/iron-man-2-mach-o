# Reuse-first plan for Iron Man 2 Mach-O

## Source research

- MetalSyntax/ios-loader-boilerplate: native PS Vita Mach-O loader, import binding, ARM ABI assembly trampolines, Darwin/ObjC/GLES/audio shims and `tools/macho_prep.py`. Source: https://github.com/MetalSyntax/ios-loader-boilerplate
- MetalSyntax/Zombie-Infection-vita: relevant Gameloft Vita wrapper example: https://github.com/MetalSyntax/Zombie-Infection-vita
- touchHLE: HLE implementations for early iOS UIKit/Foundation/GLES/OpenAL; **its CPU emulation approach is not directly a native Vita loader**: https://github.com/touchHLE/touchHLE

## Verified against original Iron Man 2 1.0.0 IPA (local only)

- Thin ARMv6 Mach-O MH_EXECUTE, cryptid=0.
- LC_DYSYMTAB dynamic undefined symbols: **490 unique entries**, not 1,190 (the latter was a broad N_UNDF scan).
- Of 490: 77 begin `_gl`, 7 `_objc`, 46 `__Z` (C++ mangling).
- 5,225 external relocation records: type ARM_RELOC_VANILLA, extern=1, length=2 (4-byte), pcrel=0.
- 1,696 indirect symbol entries.

## Implementation order

1. Run upstream `macho_prep.py` on the original decrypted executable locally; **inspect exact output format** before consuming `rebase.bin`.
2. Use upstream Mach-O loader/assembly glue under its actual license, not a guessed reimplementation. Preserve `r9`, iOS softfp ABI and stack-alignment bridge.
3. Compare the 490 imports against upstream shim exports; implement missing imports only when boot logs require them.
4. For missing UIKit/Foundation behavior, consult touchHLE implementations; adapt selectively with license review.
5. Host checks → Vita ARM build → real hardware logging. No gameplay claim until actual device proof.

## Scope

Do not commit copyrighted IPA/assets. Do not modify `Gh0sTRX/Iron-man-2-os`. This research is not evidence of a working port.
