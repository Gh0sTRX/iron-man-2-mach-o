#!/usr/bin/env python3
"""Source-level regression for original iOS ObjC external relocation handling."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
s=(root/"vendor/ios-loader-boilerplate/source/macho.c").read_text()
checks={
 "external relocations inspected":"dysym->extreloff+(uint64_t)dysym->nextrel*8u" in s,
 "classrefs treated as data":"!is_class || imp->data" in s,
 "unknown classes fail closed":"if(external.unresolved_objc_classes)" in s,
 "ObjC class symbol recognized":'_OBJC_CLASS_$_' in s,
 "ObjC metaclass symbol recognized":'_OBJC_METACLASS_$_' in s,
 "32-bit vanilla relocation only":"(bits&0x06000000u)!=0x04000000u" in (root/"vendor/ios-loader-boilerplate/source/macho_reloc.c").read_text(),
 "nil classref rejected":'Unbound ObjC class reference' in s,
 "classrefs section scanned":'"__objc_classrefs"' in s and '"__objc_superrefs"' in s,
 "no generic class stub":'make_unimpl_data(name)' not in s,
}
for k,v in checks.items():print(("PASS" if v else "FAIL"),k)
sys.exit(0 if all(checks.values()) else 1)
