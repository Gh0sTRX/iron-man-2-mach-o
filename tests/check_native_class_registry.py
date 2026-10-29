#!/usr/bin/env python3
"""Fail fast if native Objective-C class imports and CMake registrations drift.

The class table is populated at runtime, so ordinary ELF import-table tests
alone cannot detect a stale array capacity or missed initializer.
"""
from pathlib import Path
import re
import sys

base=Path(__file__).resolve().parents[1]
src=(base/"vendor/ios-loader-boilerplate/source/objc.c").read_text()
cmake=(base/"vendor/ios-loader-boilerplate/CMakeLists.txt").read_text()
runner=(base/"tools/test_source.sh").read_text()
block=re.search(r"static const char \*names\[\]=\{(.+?)\};",src,re.S)
assert block,"Native ObjC class import names missing"
names=re.findall(r'"(_OBJC_(?:META)?CLASS_\$_[a-zA-Z0-9_]+)"',block.group(1))
assert len(names)%2==0 and names,"Incomplete class/metaclass pairs"
for i in range(0,len(names),2):
    cls=names[i].removeprefix("_OBJC_CLASS_$_")
    assert names[i+1]=="_OBJC_METACLASS_$_"+cls,cls
capacity=re.search(r"static import_entry class_imports\[(\d+)\];",src)
assert capacity and int(capacity.group(1))==len(names)+1,(capacity,len(names))
fill=re.search(r"for\(size_t i=0;i<(\d+);i\+\+\) \{",src)
assert fill and int(fill.group(1))==len(names),"Class table initializer skips entries"
last=re.search(r"class_imports\[(\d+)\]=\(import_entry\)\{\"___CFConstantStringClassReference",src)
assert last and int(last.group(1))==len(names),"CFString slot must follow class pairs"
lookup=re.search(r"for\(size_t i=0;i<(\d+);i\+\+\)if\(class_imports\[i\]\.name",src)
assert lookup and int(lookup.group(1))==len(names)+1,"Class lookup scans wrong range"
new_modules={"url":"im2_url_init(root,foundation_failure)",
             "url_request":"im2_url_request_init(root,foundation_failure)",
             "file_manager":"im2_file_manager_init(root)"}
for name,init in new_modules.items():
    assert init in src,("Missing startup initializer",name)
    assert f"source/{name}.c" in cmake,("Missing VitaSDK source",name)
    assert f"tests/test_{name}.c" in runner,("Missing host regression test",name)
assert "im2_uikit_hierarchy_init(root,foundation_failure)" in src,"UIKit superclass init missing"
assert "source/uikit_hierarchy.c" in cmake,"UIKit module not in ARM ELF"
assert "tests/test_uikit_hierarchy.c" in runner,"UIKit host tests not wired"
assert "im2_uikit_application_init(root,foundation_failure)" in src,"UIApplication state init missing"
assert "source/uikit_application.c" in cmake,"UIApplication not in Vita ELF"
assert "tests/test_uikit_application.c" in runner,"UIApplication tests not wired"
print(f"PASS: {len(names)//2} native Objective-C classes, class/metaclass imports, CFString entry, new module startup and test wiring")
sys.exit(0)
