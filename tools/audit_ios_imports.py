#!/usr/bin/env python3
"""Compare original iOS import names with actual loader import table definitions.

Usage:
  python3 tools/audit_ios_imports.py /owner/ironman2_import_audit.json vendor/ios-loader-boilerplate/source
  python3 tools/audit_ios_imports.py /owner/imports.txt vendor/ios-loader-boilerplate/source
Original game executable and assets are intentionally NOT stored in the repository.
"""
import argparse
import json
from pathlib import Path
import re
import sys

def table_names(source_dir):
    provided=set()
    for path in sorted([*source_dir.glob("*.c"), *source_dir.glob("*.cpp")]):
        data=path.read_text(encoding="utf8",errors="replace")
        data=re.sub(r"/\*.*?\*/","",data,flags=re.S)
        data=re.sub(r"//[^\n]*","",data)
        for block in re.finditer(r"const\s+import_entry\s+\w+_imports\s*\[\s*\]\s*=\s*\{(.*?)\};",data,re.S):
            table=block.group(1)
            provided.update("_"+m.group(1) for m in
                            re.finditer(r"\b(?:F|D|V|F_ABI)\s*\(\s*([A-Za-z_]\w*)",table))
            provided.update(m.group(1) for m in
                            re.finditer(r'"(_[A-Za-z_][A-Za-z0-9_$]*)"',table))
        binder=re.search(r"static\s+void\s+bind_class_imports\s*\(void\)\s*\{(.*?)^\}",data,re.S|re.M)
        if binder and 'IM2_IMPORT_DATA' in binder.group(1):
            provided.update(re.findall(r'"(_OBJC_(?:META)?CLASS_\$_[A-Za-z_][A-Za-z0-9_]*|___CFConstantStringClassReference)"',binder.group(1)))
    return provided

def original_names(path):
    if path.suffix.lower()==".json":
        data=json.loads(path.read_text(encoding="utf8"))
        if not isinstance(data.get("dynamic_imports"),list):
            raise ValueError("Expected dynamic_imports list in original audit JSON")
        return set(data["dynamic_imports"])
    return set(line.strip() for line in path.read_text().splitlines()
               if line.strip() and not line.lstrip().startswith("#"))

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("symbols",type=Path)
    ap.add_argument("source_dir",type=Path)
    ap.add_argument("--output",type=Path,help="Write JSON report")
    args=ap.parse_args()
    if not args.source_dir.is_dir():ap.error("Missing loader source directory")
    wanted=original_names(args.symbols)
    exports=table_names(args.source_dir)
    missing=sorted(wanted-exports)
    result={
        "original_imports":len(wanted),
        "loader_table_names":len(exports),
        "matched_original_imports":len(wanted&exports),
        "unresolved_original_imports":len(missing),
        "unresolved_symbols":missing,
        "note":"Name coverage only: not implementation correctness, Vita linkage or runtime readiness"
    }
    print(json.dumps(result,indent=2))
    if args.output:args.output.write_text(json.dumps(result,indent=2)+"\n")
    return 1 if missing else 0

if __name__=="__main__":sys.exit(main())
