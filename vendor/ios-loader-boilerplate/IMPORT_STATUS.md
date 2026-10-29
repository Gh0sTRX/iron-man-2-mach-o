# Vendored loader source audit — 2026-10-29

Upstream: https://github.com/MetalSyntax/ios-loader-boilerplate (master)
This is an **incomplete source import** for technical evaluation, not a working loader.

## Imported files
source/macho.c, macho.h, imports.c, imports.h, darwin.c, darwin.h,
asm.S, log.c, log.h, objc.c, objc.h, gles.c, gles.h,
coreaudio.c, input.c, input.h, patch.c, patch.h, hle.h,
cgbi.c, vitagl_no_splash.c.

## Not imported
source/main.c and source/darwin_printf.c were blocked by the connector.
The upstream root LICENSE path was not found. Check the upstream project's
license/redistribution terms before distributing copied code or binaries.
Upstream vendored vitaGL sources and LiveArea resources are not present.

## Known compile and runtime blockers
- Mach-O segment_command layout must include cmd/cmdsize.
- Section field names in macho.c must match section struct.
- Double-slide of rebased pointers and constructors must be corrected.
- VM mapping and executable permissions/cache coherency require real Vita test.
- kubridge header, kubridge_stub and runtime kernel plugin are not installed here.
- Full Objective-C/UIKit/Darwin/GLES/audio import compatibility is not proven.
- The original game's 490 imports and 297 constructors are not tested.

## Build status
Existing GitHub Actions workflow builds only the standalone ARM probe.
This vendor directory is NOT yet included in a successful VitaSDK loader build.
