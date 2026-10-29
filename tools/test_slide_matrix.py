#!/usr/bin/env python3
"""Regression: original IPA + RBS1 rebases under four valid runtime bases."""
import json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
if len(sys.argv)!=3:
    raise SystemExit("Usage: test_slide_matrix.py original.ipa rebase.bin")
IPA,RBS=sys.argv[1:]
for base in (0x81001000,0x90001000,0x98001000,0xA0001000):
    p=subprocess.run([sys.executable,str(ROOT/'simulate_macho_mapping.py'),IPA,RBS,'--base',hex(base)],capture_output=True,text=True,check=True)
    d=json.loads(p.stdout)
    assert d['rebases']==32646 and d['constructors']==297
    assert int(d['first_constructor_runtime'],16)==base+0x1588
    assert d['contiguous_bytes']==5222400
    print(f"PASS base=0x{base:08x}, rebases={d['rebases']}, constructors={d['constructors']}")
