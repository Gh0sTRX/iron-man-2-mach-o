#!/usr/bin/env python3
"""Test supplied owner IPA and RBS1; intentionally does not execute guest code."""
import argparse
import importlib.util
from pathlib import Path
import struct
import zipfile

parser=argparse.ArgumentParser()
parser.add_argument("--ipa",type=Path,required=True)
parser.add_argument("--rbs1",type=Path,required=True)
args=parser.parse_args()
source=Path(__file__).resolve().parents[1]/"tools"/"preflight_original_image.py"
spec=importlib.util.spec_from_file_location("original_preflight",source)
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with zipfile.ZipFile(args.ipa) as archive:
    data=archive.read("Payload/IronMan2.app/IronMan2")
rbs=args.rbs1.read_bytes()
assert module.preflight(data,rbs)
# One RBS1 word points into __TEXT in the supported original 1.0.0 IPA.
first_slot=struct.unpack_from("<I",rbs,8)[0]
text_vm=0x1000
bad_payload=bytearray(data)
struct.pack_into("<I",bad_payload,first_slot-text_vm,0xffffffff)
cases={
    "RBS1 invalid pointer payload":(bytes(bad_payload),rbs),
    "bad Mach-O magic":(b"ABCD"+data[4:],rbs),
    "short Mach-O":(data[:12],rbs),
    "oversized command":(data[:32]+b"\xff"*4+data[36:],rbs),
    "bad RBS1 magic":(data,b"ABCD"+rbs[4:]),
    "truncated RBS1":(data,rbs[:-4]),
    "extra RBS1 bytes":(data,rbs+b"junk"),
    "zero RBS1 entries":(data,rbs[:4]+b"\0"*4+rbs[8:]),
    "unaligned RBS1":(data,rbs[:8]+struct.pack("<I",struct.unpack_from("<I",rbs,8)[0]+1)+rbs[12:]),
    "duplicate RBS1":(data,rbs[:12]+rbs[8:12]+rbs[16:]),
    "RBS1 out of image":(data,rbs[:8]+struct.pack("<I",0xfffffffc)+rbs[12:]),
}
for name,(guest,reloc) in cases.items():
    try:module.preflight(guest,reloc)
    except ValueError:print("PASS rejected",name)
    else:raise AssertionError("Unexpectedly accepted "+name)
print("PASS: original image plus",len(cases),"negative corruption tests")
