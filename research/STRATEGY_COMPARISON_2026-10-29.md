# 2026-10-29 — RBS1 audit and development strategy comparison

## New reproducible result
Run `python3 tools/audit_rbs1_quality.py original.ipa rebase_conservative.bin` locally (no game files in repository). Against the original Iron Man 2 iOS 1.0.0 binary and current 35,029-entry conservative RBS1:
- 35,029 slots in file-backed segments and all original 32-bit values point within the original image range.
- 0 zero pointers; 0 out-of-image pointers; 0 invalid slot locations.
- 297 of 297 `__mod_init_func` slots are included.
**This is a necessary sanity check, not proof that all 35,029 values are real pointers.** Embedded integer values may coincidentally lie within image bounds. Compare against upstream macho_prep.py and inspect high-risk sections.

## Critical loader issue (unresolved)
The upstream loader allocates memory blocks with sceKernelAllocMemBlockForVM, obtains text_ptr/data_ptr, but accesses fixed `MACHO_SLIDE + vmaddr` addresses without showing corresponding mapping. It also assumes a common slide across __TEXT and __DATA, despite allocating separate blocks. Before device execution, implement/verify actual mapping or a contiguous allocation and computed slide; preserve original segment spacing and verify executable permissions and cache flushes. Do not claim boot success yet.

## Classic reconstruction vs original Mach-O execution

Classic reconstruction: existing partially playable Vita prototype, original logic recreated piecemeal, heavy manual effort for movement, animation, combat, missions, QTE, audio. Greater control over native rendering.

Mach-O execution: preserves the original compiled game logic **if** runtime dependencies and ABI are satisfied, but requires correct ARMv6 execution, dyld imports, Objective-C/UIKit, graphics/audio, file paths, memory and initialization. Still no successful boot or measured speed advantage.

### Time accounting — do not mistake projections for measured savings
Verified time saved to date: **not measurable**; both strategies have different delivered artifacts, and Mach-O has not yet produced a playable build. Avoid percentages like '80% faster' without comparable milestones. Conditional estimate for planning only: if original gameplay logic runs, replacing months of manual reconstruction with weeks of compatibility work is plausible, but integration could also exceed reconstruction cost. Measure by identical milestones: first rendered scene, controllable character, combat, mission trigger, stable frame rate. Record engineer-hours and blockers per milestone.

## Priority
1. Fix segment parsing (patch exists).
2. Resolve real memory mapping and slide semantics, with unit tests.
3. Verify rebase classification against upstream original generator.
4. Fix constructor double-slide and validate guest call ABI.
5. Boot logging on actual Vita; only then begin missing imports/graphics.

Preserve Gh0sTRX/Iron-man-2-os; do not commit proprietary binaries or assets.
