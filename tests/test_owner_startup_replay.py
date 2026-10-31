#!/usr/bin/env python3
"""Exercise the owner replay with our own ARM instructions, no game bytes."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--elf', type=Path, required=True)
args = parser.parse_args()


def image(symbol, read_token=False):
    data = bytearray(0x4000)
    def segment(name, vm, offset, sections=0):
        return struct.pack('<II16s8I', 1, 56+sections*68, name, vm, 0x1000,
                           offset, 0x1000, 7, 3, sections, 0)
    section = struct.pack('<16s16s9I', b'__mod_init_func', b'__DATA',
                          0x2008, 4, 0x1008, 2, 0, 0, 9, 0, 0)
    strings = b'\0'+symbol.encode()+b'\0'
    commands = [segment(b'__TEXT', 0x1000, 0),
                segment(b'__DATA', 0x2000, 0x1000, 1)+section,
                struct.pack('<6I', 2, 24, 0x2100, 1, 0x2110, len(strings)),
                struct.pack('<20I', 11, 80, 0, 0, 0, 0, 0, 1,
                            0, 0, 0, 0, 0, 0, 0, 0, 0x2180, 1, 0, 0),
                struct.pack('<5I', 0x21, 20, 0, 0, 0)]
    struct.pack_into('<7I', data, 0, 0xfeedface, 12, 6, 2,
                     len(commands), sum(map(len, commands)), 0)
    data[28:28+sum(map(len, commands))] = b''.join(commands)
    struct.pack_into('<I', data, 0x1000, 0x1400)
    struct.pack_into('<I', data, 0x1008, 0x1400)
    struct.pack_into('<IBBHI', data, 0x2100, 1, 1, 0, 0, 0)
    data[0x2110:0x2110+len(strings)] = strings
    if read_token:
        code = [0xe59f0004, 0xe5900000, 0xe12fff1e, 0]
        relocation = 0x140c
    else:
        # push {r4,lr}; mov r0,#16; ldr r3,literal; blx r3; pop; literal
        code = [0xe92d4010, 0xe3a00010, 0xe59f3004, 0xe12fff33, 0xe8bd8010, 0]
        relocation = 0x1414
    struct.pack_into('<'+'I'*len(code), data, 0x400, *code)
    struct.pack_into('<2I', data, 0x2180, relocation, 0x0c000000)
    rbs = struct.pack('<4sIII', b'RBS1', 2, 0x2000, 0x2008)
    return data, rbs


with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    for symbol, read_token, expected, frameworks in (
        ('_malloc', False, None, False),
        ('_malloc', False, None, True),
        ('_im2_missing_test_only', False, 'unbound_import_execution', False),
        ('_im2_missing_test_only', True, 'unbound_import_data_access', False),
    ):
        data, rbs = image(symbol, read_token)
        macho, reloc, output = root/'synthetic', root/'rbs1', root/'report.json'
        macho.write_bytes(data)
        reloc.write_bytes(rbs)
        command = [sys.executable, 'tools/replay_owner_startup.py',
             '--elf', str(args.elf), '--macho', str(macho), '--rbs1', str(reloc),
             '--output', str(output), '--drain-exit-callbacks']
        if frameworks:
            command.append('--initialize-frameworks')
        result = subprocess.run(command, capture_output=True, text=True)
        assert result.returncode == 0, result.stdout+result.stderr
        report = json.loads(output.read_text())
        assert report['frameworks_initialized'] == frameworks
        assert report['framework_data_bindings'] == (67 if frameworks else 0)
        assert report['external_relocations_applied'] == 1
        assert report['constructors_available'] == 1
        assert len(report['constructors_entered']) == 1
        assert not report['main_called'] and not report['physical_vita_verified']
        if expected:
            assert report['status'] == 'stopped' and report['failure']['reason'] == expected, report
            assert report['failure']['symbol'] == symbol
            assert report['constructors_returned'] == 0
        else:
            assert report['status'] == 'all_constructors_returned', report
            assert report['constructors_returned'] == 1
            assert report['exit_callbacks_drained'] and report['exit_callbacks_entered'] == 0
            assert report['imports_called'][0]['name'] == '_malloc'
print('PASS: synthetic original-format ARM constructor, native relocation/call_guest, real import thunk and both unsupported code/data traps')
