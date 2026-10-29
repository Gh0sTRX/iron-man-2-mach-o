#!/usr/bin/env python3
"""Host-side regression tests for upstream Mach-O loader assumptions. No Vita execution."""
import argparse
import json
import struct
import zipfile

def inspect(binary):
    if binary[:4] != bytes.fromhex("cefaedfe"):
        raise ValueError("Expected 32-bit little-endian Mach-O")
    ncmds = struct.unpack_from("<I", binary, 16)[0]
    off = 28
    segments = []
    constructors = []
    for _ in range(ncmds):
        cmd, size = struct.unpack_from("<II", binary, off)
        if size < 8 or off + size > len(binary):
            raise ValueError("Malformed load command")
        if cmd == 1:
            wrong = binary[off:off + 16].split(b"\0")[0].decode("latin1")
            correct = binary[off + 8:off + 24].split(b"\0")[0].decode("ascii")
            vm, vmsize, fileoff, filesize, _, _, nsects, _ = struct.unpack_from("<8I", binary, off + 24)
            segments.append({"name": correct, "upstream_before_fix": wrong, "vmaddr": hex(vm), "vmsize": hex(vmsize)})
            for i in range(nsects):
                q = off + 56 + 68 * i
                name = binary[q:q + 16].split(b"\0")[0].decode("ascii")
                addr, length = struct.unpack_from("<II", binary, q + 32)
                if name == "__mod_init_func":
                    if addr < vm or addr + length > vm + filesize:
                        raise ValueError("Constructor section not file-backed")
                    start = fileoff + addr - vm
                    values = struct.unpack_from("<" + str(length // 4) + "I", binary, start)
                    constructors.append({"section_vmaddr": hex(addr), "count": len(values),
                                         "nonzero": sum(bool(v) for v in values),
                                         "first_values": [hex(v) for v in values[:5]]})
        off += size
    return {"segments": segments, "constructors": constructors,
            "segment_header_bug_reproduced": all(s["upstream_before_fix"] != s["name"] for s in segments),
            "warning": "Upstream macho_run_inits adds MACHO_SLIDE to values read from an already rebased __mod_init_func; verify to prevent double-slide."}

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("ipa")
    a = p.parse_args()
    with zipfile.ZipFile(a.ipa) as z:
        binary = z.read("Payload/IronMan2.app/IronMan2")
    print(json.dumps(inspect(binary), indent=2))
