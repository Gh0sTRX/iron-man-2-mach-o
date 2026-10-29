#!/usr/bin/env python3
"""Read-only audit of dynamic imports and relocations from a legally owned 32-bit Mach-O."""
import argparse
import collections
import json
import pathlib
import struct
import zipfile


def analyze(data):
    if data[:4] != bytes.fromhex("cefaedfe"):
        raise ValueError("Expected thin little-endian 32-bit Mach-O")
    _, cpu, subtype, kind, ncmds, cmdbytes, _ = struct.unpack_from("<IiiIIII", data)
    offset = 28
    symtab = dysymtab = None
    if offset + cmdbytes > len(data):
        raise ValueError("Truncated load commands")
    for _ in range(ncmds):
        cmd, size = struct.unpack_from("<II", data, offset)
        if size < 8 or offset + size > len(data):
            raise ValueError("Invalid load command")
        if cmd == 2:
            symtab = struct.unpack_from("<IIII", data, offset + 8)
        elif cmd == 11:
            dysymtab = struct.unpack_from("<18I", data, offset + 8)
        offset += size
    if not symtab or not dysymtab:
        raise ValueError("Missing symbol tables")
    symoff, nsyms, stroff, strsize = symtab
    first, count = dysymtab[4:6]
    _, nindirect = dysymtab[12:14]
    reloff, nreloc = dysymtab[14:16]
    if (first + count > nsyms or symoff + 12 * nsyms > len(data)
            or stroff + strsize > len(data) or reloff + 8 * nreloc > len(data)):
        raise ValueError("Table bounds invalid")
    imports = []
    for i in range(first, first + count):
        stridx = struct.unpack_from("<I", data, symoff + i * 12)[0]
        if stridx >= strsize:
            raise ValueError("Invalid string index")
        end = data.find(b"\0", stroff + stridx, stroff + strsize)
        if end == -1:
            raise ValueError("Unterminated symbol")
        imports.append(data[stroff + stridx:end].decode("latin1"))
    reloc_types = collections.Counter()
    for i in range(nreloc):
        _, info = struct.unpack_from("<II", data, reloff + 8 * i)
        reloc_types[(info >> 28, (info >> 27) & 1, (info >> 25) & 3, (info >> 24) & 1)] += 1
    return {
        "cpu": cpu, "subtype": subtype, "filetype": kind,
        "total_symbols": nsyms, "dynamic_import_count": count,
        "dynamic_imports": imports, "indirect_symbol_count": nindirect,
        "external_relocation_count": nreloc,
        "external_relocation_types": [
            {"type": t, "extern": ext, "length": length, "pcrel": pc, "count": n}
            for (t, ext, length, pc), n in sorted(reloc_types.items())
        ],
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("--ipa", action="store_true")
    parser.add_argument("--entry", default="Payload/IronMan2.app/IronMan2")
    parser.add_argument("--output")
    args = parser.parse_args()
    binary = (zipfile.ZipFile(args.input).read(args.entry)
              if args.ipa else pathlib.Path(args.input).read_bytes())
    report = json.dumps(analyze(binary), indent=2)
    if args.output:
        pathlib.Path(args.output).write_text(report + "\n")
    else:
        print(report)
