# Iron Man 2 iOS Mach-O loader — readiness audit (2026-10-29)

Branch: `experiment/ios-macho-loader`. Main Vita port untouched.

## Source and toolchain
- Imported 21 upstream loader sources + added `darwin_printf.h`, `cgbi.h`.
- Still missing upstream `source/main.c` and `source/darwin_printf.c`; importing those through the connector was blocked.
- New object-only VitaSDK CMake target: `vendor/ios-loader-boilerplate/CMakeLists.txt`.
- New **no VPK** compile workflow: `.github/workflows/loader-readiness.yml`.
- Build dependencies available as official VitaSDK 2026.08 packages: `kubridge`, `vitaGL`, `vitaShaRK`, `SceShaccCgExt`, `freetype`, `libpng`, `zlib`, `bzip2`, `libmathneon`, `sdl2`, `taihen`.
- Full source compilation in GitHub Actions has **not been observed passing**. On-console `kubridge.skprx` and `libshacccg.suprx` are distinct runtime requirements.

## Applied source patches
1. Correct 32-bit `LC_SEGMENT` layout and section/indirect-pointer field offsets.
2. Replace two independent segment VM allocations with one contiguous image and a runtime-computed slide.
3. Fix double-slide constructor addresses and local indirect slots.
4. Perform RBS1 before binding external imports; validate full file length, count, ordering, alignment, slot and original pointer payload ranges.
5. Validate Mach-O load command, FAT slice, segment file, symtab and indirect symbol bounds.
6. Refuse unsupported/unresolved imports and do not silently install zero-result fallback thunks.
7. Reject incomplete thunk memory allocation and out-of-range constructors; keep guest execution fail-closed until RX/cache behavior is tested.
8. Remove fixed compile-time slide from `macho.h`; update Darwin diagnostics to the runtime getter.

## Tests actually performed on owner's external original IPA
- Mach-O: ARMv6, 7,480,208-byte guest image, 26 load commands.
- Guest image span: 5,222,400 bytes.
- RBS1: 32,646 unique relocation entries, source pointer values checked.
- Indirect symbols: 401 local slots all in RBS1; 906 external slots all excluded.
- 4 host-side slide simulations: passed; 297 constructors were enumerated.
- 14 existing isolated unit tests passed.
- 10 corrupt-file negative tests passed, plus an invalid-pointer-payload case also rejected.
- These are **host checks only**, not Vita or Vita3K execution.

## Original dynamic imports (strict name-only coverage)
- Original undefined symbols: **490**.
- Vendored symbol name definitions across Darwin, GLES, CoreAudio, ObjC and patch tables: **221**.
- Original symbols matched by name: **150**.
- Missing by name: **340**, including 66 Objective-C class data imports, 37 GLES entries, 32 CoreFoundation/CoreGraphics entries, 6 Objective-C metaclasses and more.
- Matching names is NOT equivalent to functioning compatibility or successful runtime ABI calls.

## Strict pre-VPK gate (not yet passed)
1. All required loader source files present and redistributable; no undocumented dependency omissions.
2. VitaSDK full ARM source compile and actual link without missing imports/plugins.
3. 490/490 original imports accounted for with correct data/function classification AND behavior.
4. RBS1 and constructors verified under runtime slide without accidental double relocations.
5. Guest-executable memory mapping and I/D cache correctness demonstrated on real Vita hardware.
6. Real device/Vita3K bootstrap, graphics, input, CoreAudio and fallback paths validated.
7. Required game-owned assets handled externally, never committed to Git.
8. Only after passing the above: create and validate actual VPK.

This report is a checklist of evidence, not a statement that the Vita port can already boot the original iOS binary.
