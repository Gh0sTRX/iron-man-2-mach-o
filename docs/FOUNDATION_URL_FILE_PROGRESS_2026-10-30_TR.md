# Foundation URL / request / NSFileManager follow-up (2026-10-30)

Branch: `experiment/ios-macho-loader`. This is the **original Mach-O loader**, not the independent VitaGL game reimplementation.

## Implemented source (not verified on a physical PS Vita)

- `url.c/h`: native `NSURL` with `URLWithString:`, `fileURLWithPath:`, `initWithString:`, `initFileURLWithPath:`, `absoluteString`, `relativeString` (absolute-only subset), `scheme`, `path`, `isFileURL`, `copy` and object lifetime. Embedded NUL, invalid percent encodings and remote file authorities fail; root `/` is a valid file URL; URL-length limit is explicit. No URL loading is implied.
- `url_request.c/h`: native `NSURLRequest` and `NSMutableURLRequest` with `requestWithURL:`, `initWithURL:`, `URL`, `HTTPMethod`, `HTTPBody`, `valueForHTTPHeaderField:`, `allHTTPHeaderFields`, immutable `copy`, `mutableCopy`, `setURL:`, `setHTTPMethod:`, `setHTTPBody:`, `setValue:forHTTPHeaderField:`. URL/method/body/header references retain and release real runtime objects. HTTP header lookup is ASCII case-insensitive; dictionary snapshots are separate values.
- `file_manager.c/h`: native `NSFileManager` singleton supporting `defaultManager` and real `fileExistsAtPath:`. It uses the same mapped filesystem paths as `NSData`, including directory stat and rejecting embedded NUL strings.
- `collections.c`: removed array/dictionary entries now clear the vacated slot, instead of retaining stale pointers.
- Registered class/metaclass data imports and added sources to the VitaSDK CMake object and linked ELF targets.

## New test coverage (wired, not confirmed executed this turn)

- `tests/test_url.c` — actual registered native IMP calls, local/remote URLs, encoding, ownership and malformed inputs.
- `tests/test_url_request.c` — URLRequest immutable vs mutable methods, ownership of URL/method/body, header replacement/removal, snapshot isolation, copy semantics.
- `tests/test_file_manager.c` — actual stat after creating/removing a temp file, custom iOS path mapping, directories, nil and binary-NUL filenames.
- `tools/test_source.sh` compiles these production modules with host ASan/UBSan.
- VitaSDK `CMakeLists.txt` now includes all three modules; GitHub workflow invokes full ARM ELF compilation, but **latest CI results were not independently read/verified**.

## Original-image evidence and limits

The actual owner's Mach-O contains undefined class import names for **all four newly registered classes**: `NSURL`, `NSURLRequest`, `NSMutableURLRequest`, `NSFileManager`. Corresponding metaclass import names were not found among the original 490 undefined imports, although real host metaclass identities are registered for dispatch. Static name coverage alone is not behavior/ABI evidence.

Class inventory now marks **12 of 64 external framework classes partial** and **52 not implemented**. The previous verified name audit was 184/490 before these classes; re-run the exact scanner for a new authoritative total.

The original app also references `requestWithURL:cachePolicy:timeoutInterval:` and `initWithURL:cachePolicy:timeoutInterval:`. These are **intentionally NOT implemented**. Apple's ARMv6 ABI documents `double` as 4-byte naturally aligned whereas Vita EABI assumes 8-byte argument alignment; the current generic native bridge cannot be assumed correct for that signature. Build and test a dedicated conversion bridge before enabling these selectors.

Neither these classes nor these tests implement NSURLConnection/CFNetwork, UIKit UIApplicationMain, GLES frame rendering or Vita executable memory verification. No guest game instructions were executed in this change. No SELF/VPK produced.

## Immediate next blockers

1. Run current CI and fix real ARM / ASan compile or logic failures (do not infer success from the previously linked ELF).
2. Add deterministic double/64-bit Darwin ARMv6 argument ABI test against production assembly before adding timeout selectors.
3. Continue `NSURLConnection`, UIKit/bootstrap and missing imports only with concrete behavior, real owner selector inventory and tests.
4. Preserve fail-closed runtime and VPK release evidence gate until hardware boot is demonstrated.
