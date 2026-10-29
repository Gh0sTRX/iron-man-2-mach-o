# Relocation validation: original Iron Man 2 1.0.0 Mach-O

Read-only inspection of the owner-provided IPA's `Payload/IronMan2.app/IronMan2`. No proprietary executable committed.

## Confirmed
- 5,225 external relocations; all ARM_RELOC_VANILLA, extern=1, 32-bit width, non-PC-relative.
- **All 5,225 relocation target addresses are within the file-backed `__DATA` segment.** No out-of-bounds or unaligned targets were found.
- 3,797 relocation entries point into LC_DYSYMTAB's 490 undefined dynamic imports.
- **1,428 relocation entries point to other symbol-table entries, including symbols defined inside the executable.** Therefore a loader must not assume every external relocation points to an undefined import.
- Examples of internally defined C++ symbols targeted by relocations: `gameswf::as_object::to_bool()`, `gameswf::as_object::get_environment()`, and `gameswf::as_object::advance(float)`. This is strong evidence of an embedded GameSWF-related implementation in the executable, not proof of complete SWF functionality.
- Initial relocation target `0x004E0220` belongs to `__DATA`.

## Engineering implications
- Separate relocation handling for imported vs. internally defined symbols.
- Derive runtime address using segment mapping plus relocation addends; verify original stored pointer and symbol value before patching.
- Test relocation application on a *copy* of mapped memory, never patch the original IPA.
- Do not assume that a relocation being type VANILLA means a direct base-address addition is always sufficient.
- Investigate `gameswf` as a potential original menu/UI subsystem, since `ironmanmenu.swf` exists among app assets.

## Remaining
Actual upstream MetalSyntax `macho_prep.py` compatibility and `rebase.bin` generation are **not yet verified**. Physical Vita execution is not demonstrated.
