#!/usr/bin/env python3
"""Read-only coverage report: original Mach-O ObjC class relocations vs native imports.

Run with a user-owned IPA: python3 tools/report_objc_import_coverage.py --ipa path/to/game.ipa
No game data is copied into the repository.
"""
import argparse
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_objc_classref_bindings import inspect
import zipfile

def analyze(ipa: Path, source: Path):
    with zipfile.ZipFile(ipa) as archive:
        matches = [name for name in archive.namelist()
                   if name.startswith("Payload/") and name.endswith(".app/IronMan2")]
        if len(matches) != 1:
            raise ValueError("Expected exactly one original IronMan2 executable")
        audit = inspect(archive.read(matches[0]))
    needed = {}
    for slot in audit["slots"]:
        symbol = slot.get("symbol", "")
        if slot["kind"] not in ("external_relocation", "external"):
            continue
        if not symbol.startswith(("_OBJC_CLASS_$_", "_OBJC_METACLASS_$_")):
            continue
        needed[symbol] = needed.get(symbol, 0) + 1
    text = source.read_text(encoding="utf-8")
    # These are the explicitly registered static class imports, not proof that
    # an imported class implements its methods or is ABI-compatible.
    declared = set(re.findall(r'"(_OBJC_(?:META)?CLASS_[$]_[A-Za-z0-9_]+)"', text))
    missing = {key: count for key, count in sorted(needed.items()) if key not in declared}
    return {"required_symbols": len(needed),
            "required_slots": sum(needed.values()),
            "declared_symbols": len(set(needed) & declared),
            "missing_symbols": missing,
            "missing_slots": sum(missing.values()),
            "warning": "Static symbol declaration coverage only; not runtime linkage or gameplay."}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ipa", type=Path, required=True)
    parser.add_argument("--objc-source", type=Path, default=Path(__file__).resolve().parents[1] /
                        "vendor/ios-loader-boilerplate/source/objc.c")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = analyze(args.ipa, args.objc_source)
    payload = json.dumps(result, ensure_ascii=False, indent=2)
    print(payload)
    if args.output:
        args.output.write_text(payload + "\n", encoding="utf-8")

if __name__ == "__main__":
    main()
