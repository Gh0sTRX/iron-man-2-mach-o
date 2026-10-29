# Vita build dependencies — experimental Mach-O loader

## Reused build environment
The original Iron Man 2 Vita port uses `vitasdk/vitasdk:2026.08` and links `SDL2`, `vitaGL`, `vitashark`, `SceShaccCgExt`, `mathneon` and Vita system stubs. This experiment reuses that Docker image for compilation; it does **not** copy binary libraries into Git.

## Minimum probe
- VitaSDK 2026.08 with `arm-vita-eabi-gcc`, CMake Vita integration, `vita-elf-create`, `vita-make-fself`, `vita-pack-vpk`
- `SceLibKernel_stub`, `SceLibc_stub`, `SceCtrl_stub` (linked by `tests/vita_exec_probe`)
- No original iOS assets required for probe

## Future loader dependencies
- `kubridge`: kernel bridge used by upstream iOS loader, **not required for the minimal probe**
- `vitaGL`, `vitashark`, `SceShaccCgExt`: graphics stack; **not required for the minimal probe**
- `SDL2`: possible input/audio compatibility; **not required for the minimal probe**
- iOS Objective-C and system API compatibility shims: **not supplied by VitaSDK**
- Original IPA game assets: **not copied into this repository**

## Status
GitHub workflow file committed. A successful build run and produced VPK have not yet been verified. The user should check GitHub Actions logs and the `im2-exec-probe-vpk` artifact.
