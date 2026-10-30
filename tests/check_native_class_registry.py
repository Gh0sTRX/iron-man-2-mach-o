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
assert "F(UIApplicationMain, ui_application_main)" in src,"UIApplicationMain import must have a real staged bootstrap"
assert "im2_uikit_launch_once(argc,argv,principal_class_name," in src,"Missing delegate launch stage"
assert "source/uikit_bootstrap.c" in cmake,"Bootstrap not compiled for Vita ELF"
assert "tests/test_uikit_bootstrap.c" in runner,"Bootstrap behavioral tests absent"
assert 'im2_uikit_screen_init(root,(Im2ScreenGeometry){320.f,480.f})' in src, "Owner UIScreen geometry registration missing"
assert "source/uikit_screen.c" in cmake, "UIScreen missing from Vita CMake"
assert "tests/test_uikit_screen.c" in runner, "UIScreen native CGRect tests absent"
assert "im2_eagl_init(root,foundation_failure)" in src, "Original EAGL classes not initialized"
assert "gles_eagl_ready,gles_eagl_storage,gles_eagl_present" in src, "VitaGL EAGL callback not wired"
assert "source/eagl_bridge.c" in cmake, "EAGL classes not linked into Vita ARM loader"
assert "tests/test_eagl_bridge.c" in runner, "EAGL native host ABI tests absent"
imports=(base/"vendor/ios-loader-boilerplate/source/imports.c").read_text()
assert "{eagl_imports, &eagl_imports_count}" in imports, "EAGL data imports not routed through actual loader"
eagl=(base/"vendor/ios-loader-boilerplate/source/eagl_bridge.c").read_text()
for expected in ("_kEAGLColorFormatRGB565",
    "_kEAGLDrawablePropertyColorFormat","_kEAGLDrawablePropertyRetainedBacking"):
    assert expected in eagl, ("Missing owner EAGL NSString data symbol",expected)
assert "target !=IM2_GL_RENDERBUFFER_OES" in eagl or "target!=IM2_GL_RENDERBUFFER_OES" in eagl, "Unsupported renderbuffer target not rejected"




print(f"PASS: {len(names)//2} native Objective-C classes, class/metaclass imports, CFString entry, new module startup and test wiring")
sys.exit(0)
