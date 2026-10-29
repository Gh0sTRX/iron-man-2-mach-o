#!/usr/bin/env python3
"""Synthetic import parser regression tests; no original game assets needed."""
import importlib.util
from pathlib import Path
import tempfile

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("imports_audit",root/"tools"/"audit_ios_imports.py")
audit=importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
with tempfile.TemporaryDirectory() as td:
    src=Path(td)
    (src/"darwin.c").write_text("""
const import_entry darwin_imports[] = {
    F(malloc, malloc),
    D(glBindTexture),
    V(__stderrp, &stderr_ptr),
    F_ABI(objc_msgSend, assembly_entry),
    {"_OBJC_CLASS_$_SomeClass", (uintptr_t)&some_class, 1},
    // F(not_an_import, NULL)
    /* F(also_not_an_import, NULL) */
};
""")
    have=audit.table_names(src)
    assert "_malloc" in have
    assert "_glBindTexture" in have
    assert "___stderrp" in have
    assert "_OBJC_CLASS_$_SomeClass" in have
    assert "_not_an_import" not in have
    assert "_also_not_an_import" not in have
    assert "_objc_msgSend" in have
    assert len(have)==5,have
    expected=src/"original.json"
    expected.write_text('{"dynamic_imports":["_malloc","_glBindTexture","_missing"]}')
    requested=audit.original_names(expected)
    assert len(requested&have)==2
    assert requested-have=={"_missing"}
    print("PASS: F/D macros, literal data imports, comment suppression and JSON import list")
