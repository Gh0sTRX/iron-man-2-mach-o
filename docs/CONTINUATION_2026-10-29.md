# Continuation status — 2026-10-29, no VPK

## This batch (source changes)
- Added missing upstream `source/main.c` and `source/darwin_printf.c`; included both in object-only VitaSDK CMake.
- Updated `main.c` to use the dynamically calculated VM slide and report guest-load rejection clearly.
- Added five GLES1 integer-only import names: `glColorMask`, `glCullFace`, `glDepthFunc`, `glFrontFace`, `glPixelStorei`. These use pointer/integer ABI; floating-point imports still need dedicated bridges.
- Added eleven Darwin C library bindings: `strchr`, `strrchr`, `strcat`, `strncat`, `strncmp`, `strcspn`, `strpbrk`, `strtol`, `strtoul`, `strerror`, `strdup`. `strtod` was deliberately excluded due to floating-point return ABI.
- Import audit now recognizes `V(...)` data bindings (such as `__stderrp` and `__stdinp`) in addition to `F` and `D`.
- Added/expanded synthetic regression checks and upstream provenance note.

## Local verification (actual owner IPA, no Vita execution)
- 32-bit ARMv6 original guest image (7,480,208 bytes), 26 Mach-O load commands.
- 32,646 RBS1 words: complete host-side validation succeeded.
- 401 local indirect slots in relocation set; 906 external slots not rebased.
- Ten malformed image/RBS1 test cases rejected.
- An additional corrupted RBS1 *pointer payload* was rejected.
- New name coverage is **estimated** as 168/490 (previous 150 + 18 original names newly represented, including two Darwin data imports). This is *not* ABI or behavior validation.

## Blockers before packaging
1. **VitaSDK CI ARM object compile and link are not yet verified successful.**
2. Missing ~322 original import names, including ObjC class/metaclass data, CF/CG, legacy GLES, libc++ ABI and UIKit; remaining known names still require behavior checks.
3. No real-device proof that kubridge grants guest executable memory, proper cache coherence and safe constructor execution.
4. No Vita3K/PS Vita bootstrap, rendering, audio or input validation.
5. Upstream source license has not been located: obtain redistribution permission/license before a public binary release.

No VPK is built until the user-requested readiness gates are met.
