# Iron Man 2 — iOS Mach-O → PS Vita feasibility

Independent experiment; **not** a working Vita port. Original IPA, game executable and copyrighted assets are not included. Existing gameplay rewrite is untouched.

## Measured from owner-provided iOS 1.0.0 IPA
- Executable 7,480,208 bytes; thin 32-bit little-endian ARMv6 Mach-O (MH_EXECUTE).
- LC_ENCRYPTION_INFO cryptid=0.
- __TEXT vmaddr 0x1000 / vmsize 0x4b8000; __DATA 0x4b9000 / 0x43000; __LINKEDIT 0x4fc000 / 0x236000.
- 65,500 symbol records, 1,190 undefined symbol names.
- Imports include UIKit, Foundation, OpenGLES, OpenAL, CoreGraphics, QuartzCore, AudioToolbox, ObjC and C++ runtimes.

## Reproduce locally
Extract `Payload/IronMan2.app/IronMan2` from your legally obtained IPA, then run:
```sh
python3 tools/scan_macho.py /path/to/IronMan2 > local_report.json
```
Never commit the original executable or asset data.

## Next gates
1. Validate relocation/rebase information, guest memory map and ARM ABI.
2. Compare unresolved imports with [MetalSyntax ios-loader-boilerplate](https://github.com/MetalSyntax/ios-loader-boilerplate), [Inotia2-vita](https://github.com/MetalSyntax/Inotia2-vita), and [Zombie-Infection-vita](https://github.com/MetalSyntax/Zombie-Infection-vita); inspect actual source and license before reuse.
3. Build isolated import-resolution and loader tests, then a device-side crash logger.
4. Test on real Vita hardware. A successful ARM ELF build alone does not prove gameplay.

## Known blockers
Objective-C runtime, graphics/audio API coverage, dynamic linking, memory placement, C++ ABI, physical device validation.
