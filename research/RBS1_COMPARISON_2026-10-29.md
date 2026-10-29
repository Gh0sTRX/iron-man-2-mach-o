# RBS1 comparison, 2026-10-29

An independent local implementation of MetalSyntax macho_prep.py selection logic was executed against the original user-owned Iron Man 2 1.0.0 Mach-O. This is not direct execution of upstream script, so parity remains to be verified.

Old conservative candidate: 35,029 entries. New upstream-equivalent candidate: 32,646. Intersection: 32,257. Old-only: 2,772. New-only: 389. Constructor slots: 297/297 retained. The new candidate is experimental and not cleared for hardware use.

Next priorities: establish byte-for-byte equivalence with upstream, inspect false positives in __const and exception tables, and implement actual Vita contiguous memory mapping and runtime imports. No Vita boot claimed.