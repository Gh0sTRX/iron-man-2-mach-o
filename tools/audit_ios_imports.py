#!/usr/bin/env python3
"""Audit imported iOS symbols against MetalSyntax loader import tables.
Usage: python3 tools/audit_ios_imports.py original_imports.txt loader/source
The input text file must contain one imported symbol per line.
"""
import argparse
import pathlib
import re
import sys

def main():
    p=argparse.ArgumentParser()
    p.add_argument("symbols",type=pathlib.Path)
    p.add_argument("source_dir",type=pathlib.Path)
    args=p.parse_args()
    if not args.source_dir.is_dir():
        p.error("Loader source directory does not exist; vendor MetalSyntax sources first")
    source="\n".join(f.read_text(errors="replace") for f in args.source_dir.rglob("*") if f.suffix in (".c",".h"))
    exports=set(re.findall(r'\{\s*"([^"]+)"\s*,',source))
    wanted=set(s.strip() for s in args.symbols.read_text().splitlines() if s.strip() and not s.startswith("#"))
    matched=wanted&exports
    missing=wanted-exports
    print(f"Original imports: {len(wanted)}; loader table matches: {len(matched)}; unresolved: {len(missing)}")
    for s in sorted(missing):print("UNRESOLVED",s)
    return 0 if not missing else 1

if __name__=="__main__":sys.exit(main())
