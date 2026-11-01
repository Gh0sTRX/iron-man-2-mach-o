# Iron Man 2 (iOS) → PlayStation Vita — Mach-O Loader Research

> **Status (1 November 2026): research prototype, not a playable Vita port.**
> Original Iron Man 2 ARM code has reached the app entry point and, in a separate controlled worker probe, the EAGL graphics-readiness check under Unicorn. **No original game frame, native Vita boot, menu or gameplay has been verified. No SELF/VPK is produced.**

This is an **independent compatibility/loader experiment** targeting the original 2010 iOS game executable, rather than a recreation of its gameplay. It aims to run the original 32-bit ARM Mach-O program with VitaSDK, VitaGL and carefully validated substitutes for the iOS/Darwin runtime. The separate `Iron-man-2-os` gameplay-rewrite project is not changed or used as proof that this loader runs the original game.

Original IPA, executable, assets and private diagnostic output **are not included**. Use only game files you are authorized to possess.

## Current checkpoint

| Area | Measured state | Important limit |
| --- | --- | --- |
| Original executable | iOS 1.0.0; 7,480,208-byte ARMv6 Mach-O | Analysed from privately supplied owner files |
| Dynamic imports | **349/490 names matched; 141 names missing** | Name matches do **not** establish correct ABI or behavior; not a completion percentage |
| Required external ObjC classes | **27/64 partially declared; 37 not implemented** | Partial classes still have missing methods |
| Original ObjC metadata | **25 guest classes and 165 CFStrings** registered in diagnostic replay | Not a complete ObjC/iOS runtime |
| Static initialization | **297/297 original constructors** returned in Unicorn | Controlled emulation with modelled OS services |
| Application entry | Original `_main` executed; stops at `pthread_create` boundary | Main and worker do not run concurrently |
| Original worker | Entry `0x51984` separately executed through directory and console setup to `gles_eagl_ready` | Stops *before* returning graphics readiness; no fake GPU success |
| Loader build | **53 C/C++/ASM units** linked as a VitaSDK ARM ELF | An ELF build is not a Vita device boot |
| Physical PS Vita / GPU / menu | **Not verified** | No rendered game frame or playable build |

The latest original-worker evidence is in [the 1 November graphics-boot report](docs/FS_GRAPHICS_BOOT_PROGRESS_2026-11-01_TR.md) and its [machine-readable record](docs/FS_GRAPHICS_BOOT_EVIDENCE_2026-11-01.json).

### What is actually running?

In a local Unicorn ARM diagnostic, the loader prepares Mach-O mappings/relocations and framework metadata, executes the original static constructors, and enters original `_main`. The program requests a new pthread. Because the diagnostic does **not** implement concurrent guest scheduling, it stops at this boundary.

A separate, explicitly requested `--probe-worker` run starts the **original** worker entry point on another modelled stack while main is paused. The original worker passes `CFBundle`/path setup, `chdir`, ObjC opacity calls and `printf`, then reaches `gles_eagl_ready` through `context_set_current`. The probe stops because real VitaGL/GPU initialization is unavailable in that environment. This is **not** an original game frame or evidence of a functional Vita renderer.

One guest ObjC class-reference validation issue at `0x004e4d50` also remains. Experimental trap handling is opt-in; strict production validation is not bypassed.

## Implemented research components

- **Mach-O loader:** segment bounds, relocation/rebase and import-routing checks; ARM/Thumb calling-convention and constructor tests.
- **Darwin / Objective-C / Foundation / UIKit subsets:** object lifetime, selectors, collections, strings, bundles, URLs, screen/view ownership and touch-event bridge scaffolding. These are **partial**, not Apple framework replacements.
- **Legacy C/C++ ABI bridges:** string/stream/container/math/time/runtime helpers, with ARM tests for relevant old iOS calling conventions.
- **OpenGL ES / EAGL bridge scaffolding:** GLES import routing, client-array/state/texture paths, EAGL layer/context plumbing and VitaGL backend integration work. Actual GPU rasterization is **not verified**.
- **Thread/run-loop work:** thread lifecycle ABI conversions, per-thread NSTimer queues, and a **default-mode timer-only** `CFRunLoopRunInMode` subset. General run-loop sources, full scheduling and worker concurrency are open.
- **Virtual filesystem:** `chdir`, `getcwd`, path mapping and real directory-status checks; Vita `ux0:`, `ur0:` and `app0:` mounts are preserved. The owner-app diagnostic validates its app directory, **not** general game-asset loading.
- **Console bridge and UI state:** ARMv6 packed-argument `printf` handling plus `UIView` / `CALayer` opacity state needed by the original worker.

For more precise test boundaries see [run-loop/worker progress](docs/RUNLOOP_WORKER_PROGRESS_2026-11-01_TR.md) and [filesystem/graphics-startup progress](docs/FS_GRAPHICS_BOOT_PROGRESS_2026-11-01_TR.md).

## Testing and reproducibility

The project contains host ASan/UBSan checks, ARM/Thumb bridge tests, synthetic loader/replay tests and VitaSDK ELF link checks.

**Most recent confirmed passing source CI:** [GitHub Actions run #41305723253](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41305723253) (source commit `9a75c43`). This CI run did **not** execute the privately held original game executable. Original-code replay findings come from separate local experiments.

From a local checkout:

```sh
bash tools/test_source.sh

# With VitaSDK configured:
bash tools/build_loader_elf.sh
bash tools/build_arm_dispatch_test.sh
python3 tests/test_elf_imports.py --elf build/loader-full/im2_ios_loader

# With Unicorn and the ARM dispatch test binary available:
python3 tests/test_arm_dispatch.py \
  --binary build/arm-dispatch/glue.bin \
  --symbols build/arm-dispatch/symbols.txt
```

To inspect a **locally owned** original iOS 1.0.0 executable:

```sh
python3 tools/scan_macho.py /path/to/IronMan2 > local_report.json
```

A controlled original-worker replay, when you have your own original executable, RBS1 data, built ELF and Unicorn available:

```sh
python3 tools/replay_owner_startup.py \
  --elf build/loader-full/im2_ios_loader \
  --macho /path/to/IronMan2.app/IronMan2 \
  --rbs1 /path/to/rebase.bin \
  --app-dir /path/to/IronMan2.app \
  --output /path/to/local-worker-probe.json \
  --initialize-frameworks --register-metadata \
  --probe-main --probe-worker
```

The replay is a **diagnostic** with deliberately modelled OS behavior; it does not make the original game runnable on Vita. Do not commit the input files, assets or private output.

## Outstanding blockers and next milestones

1. **Real VitaGL/EAGL initialization:** validate an actual Vita graphics context, surface/renderbuffer setup, clear and present; never return a fabricated success from `gles_eagl_ready`.
2. **Native guest execution:** verify physical Vita memory mapping, executable permissions, relocation slide and instruction/data-cache handling with device-side logs.
3. **Main + worker integration:** real thread scheduling/TLS/synchronization and required run-loop behavior, rather than separate emulation probes.
4. **Runtime correctness:** resolve the ObjC class-reference issue, implement needed missing imports/selectors, and validate behavior—not just symbol names.
5. **Original content pipeline:** actual asset reads, original draw path, first rendered frame, then stable menu, controls, audio and gameplay tests.
6. **Release criteria:** verify all device/runtime/graphics/input/audio and legal-source checks before considering a SELF/VPK. Only the initial source-build/link requirements of the existing VPK acceptance checklist are met.

**The next meaningful milestone is a verified first graphics frame on physical PS Vita hardware, ideally followed by the original game's own draw calls.** There is no reliable playable-build percentage or delivery date yet.

## Reports

- [What works / what remains (Turkish)](docs/KALAN_ISLER_2026-10-31_TR.md)
- [Latest worker → EAGL readiness report (1 November)](docs/FS_GRAPHICS_BOOT_PROGRESS_2026-11-01_TR.md)
- [Worker/run-loop report (1 November)](docs/RUNLOOP_WORKER_PROGRESS_2026-11-01_TR.md)
- [Original `_main` progress (1 November)](docs/ORIGINAL_MAIN_PROGRESS_2026-11-01_TR.md)
- [Name coverage and missing imports](docs/REMAINING_BINDINGS_2026-10-31.json)
- [Build evidence](docs/BUILD_EVIDENCE_2026-10-31.json)
- [Separate Linux touchHLE menu reference — **not** Vita-loader proof](docs/OWNER_MENU_REFERENCE_2026-10-30_TR.md)

## Legal / attribution

Original game binaries and assets remain the property of their respective owners. This repository does not distribute them. Review the licenses and provenance of upstream loader/compatibility components before reuse or distribution. Development and testing here do not imply endorsement by the original game publisher or Sony.
