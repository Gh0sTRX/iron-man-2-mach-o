#!/usr/bin/env python3
"""Fail-closed release gate for the original iOS Mach-O Vita loader.

This intentionally cannot be satisfied by checking only that a VPK exists.
A real-device RX/cache test, loader link and subsystem tests are required.
"""
import argparse
import json
from pathlib import Path
import sys

GATES={
    "loader_objects_compiled":"All vendored C/ASM units compile using VitaSDK",
    "loader_elf_linked":"Full loader ELF links with VitaGL, VitaShaRK, kubridge and SDK",
    "all_original_imports_bound":"Every original required iOS import has a correct binding",
    "objc_runtime_validated":"ObjC class/metaclass/selector and ARMv6 dispatch validated",
    "rbs1_on_device_verified":"RBS1 + constructors verified on actual Vita image",
    "guest_rx_cache_verified":"Executable mapping and I/D cache verified on real PS Vita",
    "graphics_input_audio_verified":"GLES, controls and audio bootstrap tested on real device",
    "legal_assets_provenance_verified":"Game-owned assets kept external; upstream code license resolved",
}
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--evidence",type=Path,required=True)
    a=p.parse_args()
    data=json.loads(a.evidence.read_text())
    failed=[]
    for key,description in GATES.items():
        item=data.get(key)
        ok=isinstance(item,dict) and item.get("passed") is True and bool(item.get("evidence"))
        print(("PASS" if ok else "BLOCK"),key,"-",description)
        if not ok:failed.append(key)
    if failed:
        print("VPK release BLOCKED:",len(failed),"unverified gates")
        return 1
    print("All declared release gates have evidence; proceed to independent artifact validation.")
    return 0
if __name__=="__main__":
    sys.exit(main())
