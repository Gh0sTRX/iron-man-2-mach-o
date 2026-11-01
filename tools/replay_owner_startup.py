#!/usr/bin/env python3
"""Owner-only, zero-slide constructor diagnostic; NEVER a native boot claim.

Runs unmodified owner instructions against the linked compatibility ELF in
Unicorn. Unsupported bindings are trap tokens, not successful implementations.
Uses the production relocation planner, constructor validator and call_guest.
The regression fixture models heap, OS services and GPU entrypoints. Main is
opt-in and unsupported imports still trap. No input bytes enter the report.
"""
import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct
import sys

from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.arm_const import *

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from preflight_original_image import preflight


def image_tables(data):
    """Parse after strict preflight; return load tables and initializer section."""
    offset = 28
    segments, indirect, constructors = [], [], []
    sym = dys = None
    for _ in range(struct.unpack_from('<I', data, 16)[0]):
        cmd, length = struct.unpack_from('<II', data, offset)
        if cmd == 1:
            name = data[offset+8:offset+24].split(b'\0')[0].decode('ascii')
            vm, size, fileoff, filesize = struct.unpack_from('<4I', data, offset+24)
            segments.append((name, vm, size, fileoff, filesize))
            for i in range(struct.unpack_from('<I', data, offset+48)[0]):
                section = offset+56+i*68
                va, size, _, _, _, _, flags, first, _ = struct.unpack_from('<9I', data, section+32)
                if flags & 255 in (6, 7):
                    indirect.append((va, size, first))
                if flags & 255 == 9:
                    if size & 3:
                        raise ValueError('Unaligned initializer section')
                    constructors.extend(range(va, va+size, 4))
        elif cmd == 2:
            sym = struct.unpack_from('<4I', data, offset+8)
        elif cmd == 11:
            dys = struct.unpack_from('<18I', data, offset+8)
        offset += length
    return segments, indirect, constructors, sym, dys


def image_sections(data):
    offset = 28
    result = []
    for _ in range(struct.unpack_from('<I', data, 16)[0]):
        command, length = struct.unpack_from('<II', data, offset)
        if command == 1:
            for index in range(struct.unpack_from('<I', data, offset+48)[0]):
                at = offset+56+index*68
                name = data[at:at+16].split(b'\0')[0].decode('ascii')
                segment = data[at+16:at+32].split(b'\0')[0].decode('ascii')
                va, size = struct.unpack_from('<II', data, at+32)
                result.append((name, segment, va, size))
        offset += length
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--macho', type=Path, required=True)
    parser.add_argument('--rbs1', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--instruction-budget', type=int, default=20_000_000)
    parser.add_argument('--drain-exit-callbacks', action='store_true',
                        help='After successful constructors, run their real registered cleanup callbacks')
    parser.add_argument('--initialize-frameworks', action='store_true',
                        help='Reset regression state and run real objc_init with explicit OS fixtures')
    parser.add_argument('--register-metadata', action='store_true',
                        help='Register original ObjC classes and CFStrings using production validators')
    parser.add_argument('--probe-main', action='store_true',
                        help='Bounded diagnostic main probe; unresolved class references remain traps')
    args = parser.parse_args()
    if args.instruction_budget <= 0:
        parser.error('instruction budget must be positive')
    if args.register_metadata and not args.initialize_frameworks:
        parser.error('--register-metadata requires --initialize-frameworks')
    if args.probe_main and not args.register_metadata:
        parser.error('--probe-main requires --register-metadata')
    data = args.macho.read_bytes()
    pre = preflight(data, args.rbs1.read_bytes())
    segments, indirect, initializer_slots, sym, dys = image_tables(data)
    owner_sections = image_sections(data)
    old_argv = sys.argv
    try:
        sys.argv = [str(ROOT/'tests/test_arm_runtime_bridges.py'), '--elf', str(args.elf.resolve())]
        state = runpy.run_path(str(ROOT/'tests/test_arm_runtime_bridges.py'))
    finally:
        sys.argv = old_argv
    # Functions retain the actual runpy module globals, rather than its returned copy.
    g = state['call'].__globals__
    u, symbols = g['u'], g['symbols']
    framework_bindings = 0
    if args.initialize_frameworks:
        # Fresh ELF state: do not register production classes over synthetic roots.
        for section in g['alloc']:
            address, offset, size = section[3:6]
            u.mem_write(address, bytes(size) if section[1] == 8 else g['binary'][offset:offset+size])
        g['live'].clear()
        g['once_seen'].clear()
        g['tls_values'].clear()
        g['tls_next'] = 1
    ranges = []
    for name, vm, size, fileoff, filesize in segments:
        if name not in ('__TEXT', '__DATA'):
            continue
        start, end = vm & ~4095, (vm+size+4095) & ~4095
        u.mem_map(start, end-start)
        u.mem_write(vm, data[fileoff:fileoff+filesize])
        ranges.append((vm, vm+size))
    heap, heap_end = 0x40000000, 0x44000000
    u.mem_map(heap, heap_end-heap)
    cursor = heap

    def allocate(size):
        nonlocal cursor
        if not 0 <= size <= heap_end-heap:
            raise ValueError('Diagnostic allocation exceeds bounded heap')
        size = max(size, 1)
        address = cursor
        cursor += (size+15) & ~15
        if cursor > heap_end:
            raise ValueError('Diagnostic heap exhausted')
        g['live'][address] = size
        return address

    g['allocate'] = allocate
    control, thunk_base, trap_base = 0x27000000, 0x28000000, 0x29000000
    for address in (control, thunk_base, trap_base):
        u.mem_map(address, 0x10000)
    reg = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)

    def native(name, values):
        sp = g['STACK']+0x8000
        u.reg_write(UC_ARM_REG_SP, sp)
        u.reg_write(UC_ARM_REG_LR, g['STOP'])
        u.reg_write(UC_ARM_REG_R9, 0x12345678)
        for r, value in zip(reg, (values+[0]*4)[:4]):
            u.reg_write(r, value)
        words = values[4:]+[0]*16
        u.mem_write(sp, struct.pack('<'+'I'*len(words), *words))
        try:
            u.emu_start(symbols[name], g['STOP'], count=args.instruction_budget)
        except Exception as ex:
            detail = ''
            if u.reg_read(UC_ARM_REG_PC) == (symbols['fatal_error'] & ~1):
                values = []
                for register in reg:
                    pointer = u.reg_read(register)
                    text = bytearray()
                    try:
                        for index in range(256):
                            byte = bytes(u.mem_read(pointer+index, 1))
                            if byte == b'\0':
                                break
                            text.extend(byte)
                        values.append(text.decode('utf-8'))
                    except Exception:
                        values.append(hex(pointer))
                detail = ' fatal arguments='+repr(values)
            raise RuntimeError(f'{name}: PC={u.reg_read(UC_ARM_REG_PC):x} LR={u.reg_read(UC_ARM_REG_LR):x}'+detail) from ex
        if u.reg_read(UC_ARM_REG_PC) != g['STOP']:
            raise RuntimeError('Instruction budget exhausted or diagnostic trap')
        if u.reg_read(UC_ARM_REG_SP) != sp or u.reg_read(UC_ARM_REG_R9) != 0x12345678:
            raise RuntimeError('Native caller stack/r9 preservation failed')
        return u.reg_read(UC_ARM_REG_R0)

    if args.initialize_frameworks:
        mutex_addresses = {symbols[name] & ~1 for name in (
            'sceKernelLockMutex', 'sceKernelUnlockMutex', 'sceKernelDeleteMutex')}
        identity_addresses = {symbols[name] & ~1 for name in (
            'sceKernelCreateMutex', 'sceKernelGetThreadId')}
        fixture_process_us = [1_000_000]
        def framework_os(uc, address, size, context):
            if address == (symbols['sceKernelGetProcessTimeWide'] & ~1):
                fixture_process_us[0] += 1000
                uc.reg_write(UC_ARM_REG_R0, fixture_process_us[0] & 0xffffffff)
                uc.reg_write(UC_ARM_REG_R1, fixture_process_us[0] >> 32)
            elif address in mutex_addresses:
                uc.reg_write(UC_ARM_REG_R0, 0)  # Single-thread fixture, no OS scheduling.
            elif address in identity_addresses:
                uc.reg_write(UC_ARM_REG_R0, 1)
            elif address == (symbols['sceAppUtilSystemParamGetInt'] & ~1):
                # Explicit Turkish-language fixture, never a physical OS query.
                if uc.reg_read(UC_ARM_REG_R0) != 1:
                    raise AssertionError('Unexpected system parameter request')
                uc.mem_write(uc.reg_read(UC_ARM_REG_R1), struct.pack('<I', 19))
                uc.reg_write(UC_ARM_REG_R0, 0)
            elif address == (symbols['stat'] & ~1):
                # Fresh-install preference path only; do not fake general file IO.
                pointer = uc.reg_read(UC_ARM_REG_R0)
                path = bytearray()
                for index in range(4096):
                    value = bytes(uc.mem_read(pointer+index, 1))
                    if value == b'\0':
                        break
                    path.extend(value)
                if not path.endswith(b'/Library/Preferences/com.gameloft.IronMan2.plist'):
                    raise AssertionError('Unmodeled stat path: '+repr(bytes(path)))
                uc.mem_write(g['OBJECT']+0x2000, struct.pack('<I', 2))
                uc.reg_write(UC_ARM_REG_R0, 0xffffffff)
            else:
                return
            uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
        u.hook_add(UC_HOOK_CODE, framework_os)
        native('darwin_init', [])
        native('objc_init', [])
        for index in range(struct.unpack('<I', u.mem_read(symbols['objc_class_imports_count'], 4))[0]):
            name, target, kind = struct.unpack('<III', u.mem_read(symbols['class_imports']+12*index, 12))
            if not name or not target or kind != 1:
                raise RuntimeError('Production framework data binding is not initialized')
            framework_bindings += 1

    symoff, nsyms, stroff, strsize = sym
    names = []
    raw_symbols = []
    for i in range(nsyms):
        row = struct.unpack_from('<IBBHI', data, symoff+i*12)
        string_index = row[0]
        if string_index >= strsize:
            raise ValueError('Symbol string index outside string table')
        end = data.find(b'\0', stroff+string_index, stroff+strsize)
        if end < 0:
            raise ValueError('Unterminated symbol name')
        names.append(data[stroff+string_index:end].decode('utf-8'))
        raw_symbols.append(row)
    bindings, missing, trace, trap_names = {}, [], [], {}
    targets = {}
    first, count = dys[4:6]
    # Each unsupported symbol owns 64 bytes, including common data addends.
    for i in range(first, first+count):
        name = names[i]
        encoded = name.encode()+b'\0'
        scratch = allocate(len(encoded))
        u.mem_write(scratch, encoded)
        entry = native('imports_lookup', [scratch])
        if entry:
            _, target, kind = struct.unpack('<III', u.mem_read(entry, 12))
        else:
            target, kind = 0, 0
        if not target:
            target = trap_base+len(missing)*64
            trap_names[target] = name
            missing.append(name)
        elif kind == 0:
            thunk = thunk_base+len(targets)*16
            u.mem_write(thunk, struct.pack('<4I', 0xe59fc000, 0xe59ff000, target, symbols['ml_align_call']))
            targets[thunk] = name
            target = thunk
        elif kind == 2:
            targets[target & ~1] = name
        elif kind != 1:
            raise ValueError('Invalid native import kind')
        bindings[i] = target
    for i, row in enumerate(raw_symbols):
        if i not in bindings and row[1] & 0x0e in (2, 14):
            bindings[i] = row[4] | (1 if row[3] & 8 else 0)

    report = {
        'schema': 1, 'macho_sha256': hashlib.sha256(data).hexdigest(),
        'elf_sha256': hashlib.sha256(g['binary']).hexdigest(),
        'preflight': pre, 'slide': 0, 'diagnostic_only': True,
        'production_loader_execution': False, 'main_called': False,
        'physical_vita_verified': False, 'gpu_verified': False,
        'os_heap_mutex_gpu_services': 'regression fixture, not native OS',
        'unbound_symbols_in_fixture': missing,
        'frameworks_initialized': args.initialize_frameworks,
        'framework_data_bindings': framework_bindings,
        'framework_os_fixtures': 'Turkish language, absent preferences, single-thread mutex/identity and deterministic monotonic process clock; no physical OS services' if args.initialize_frameworks else None,
        'unbound_class_note': 'Real objc_init ran; methods remain partial.' if args.initialize_frameworks else 'Fixture does not call full objc_init; unbound class names are not an implementation audit.',
        'constructors_available': len(initializer_slots), 'constructors_entered': [],
        'constructors_returned': 0, 'imports_called': trace,
        'exception_unwinding_verified': False,
    }
    failure = None
    phase = 'relocation'
    map_callback, resolve_callback, init_callback, init_return = (control+i*64 for i in range(4))
    metadata_read = control+256
    # After call_guest returns, restore the native initializer callback's frame.
    u.mem_write(init_return, struct.pack('<3I', 0xe59de008, 0xe28dd010, 0xe12fff1e))

    def returned(value):
        u.reg_write(UC_ARM_REG_R0, value)
        u.reg_write(UC_ARM_REG_PC, u.reg_read(UC_ARM_REG_LR))

    def fail(reason, **details):
        nonlocal failure
        failure = {'reason': reason, 'pc': hex(u.reg_read(UC_ARM_REG_PC)),
                   'lr': hex(u.reg_read(UC_ARM_REG_LR)), 'phase': phase, **details}
        u.emu_stop()

    blocked = {'_dispatch_async', '_select', '__Unwind_SjLj_Resume', '___cxa_throw',
               '___cxa_rethrow', '___cxa_begin_catch', '___cxa_end_catch'}

    def code_hook(uc, address, size, context):
        if address == metadata_read:
            va, length, executable = (uc.reg_read(r) for r in reg[1:])
            allowed = any(lo <= va and va+length <= hi for lo, hi in ranges)
            if executable:
                allowed = allowed and any(name == '__text' and start <= va and va+length <= start+size
                                          for name, segment, start, size in owner_sections)
            returned(va if allowed else 0)
        elif address == map_callback:
            va = uc.reg_read(UC_ARM_REG_R1)
            returned(va if any(lo <= va <= hi-4 for lo, hi in ranges) else 0)
        elif address == resolve_callback:
            index, out = uc.reg_read(UC_ARM_REG_R1), uc.reg_read(UC_ARM_REG_R2)
            if index in bindings:
                uc.mem_write(out, struct.pack('<I', bindings[index]))
                returned(0)
            else:
                returned(1)
        elif address == init_callback:
            fn = uc.reg_read(UC_ARM_REG_R1)
            report['constructors_entered'].append(hex(fn))
            sp = uc.reg_read(UC_ARM_REG_SP)-16
            uc.mem_write(sp, struct.pack('<4I', 0, 0, uc.reg_read(UC_ARM_REG_LR), 0))
            uc.reg_write(UC_ARM_REG_SP, sp)
            for r, value in zip(reg, (fn, 0, 0, 0)):
                uc.reg_write(r, value)
            uc.reg_write(UC_ARM_REG_LR, init_return)
            uc.reg_write(UC_ARM_REG_PC, symbols['call_guest'])
        elif address == init_return:
            report['constructors_returned'] += 1
        elif address == symbols['call_guest'] and phase == 'exit_callbacks':
            report['exit_callbacks_entered'] += 1
        elif trap_base <= address < trap_base+0x10000:
            fail('unbound_import_execution', symbol=trap_names.get(address & ~63, 'unknown'))
        elif address in targets:
            name = targets[address]
            if len(trace) < 4096:
                trace.append({'name': name, 'lr': hex(uc.reg_read(UC_ARM_REG_LR)),
                              'constructor': len(report['constructors_entered']), 'phase': phase})
            if name in blocked:
                fail('unsupported_or_placeholder_operation', symbol=name)
            if name == '_pthread_create':
                fail('native_thread_creation_boundary', symbol=name,
                     start_routine=hex(uc.reg_read(UC_ARM_REG_R2)),
                     argument=hex(uc.reg_read(UC_ARM_REG_R3)),
                     note='Single-thread diagnostic has no OS thread scheduler; production callback bridge is not replaced by fake success.')
            if name.startswith('_objc_msgSend'):
                receiver = uc.reg_read(UC_ARM_REG_R0)
                if trap_base <= receiver < trap_base+0x10000:
                    fail('unbound_class_receiver', symbol=trap_names.get(receiver & ~63, 'unknown'))

    def token_access(uc, access, address, size, value, context):
        fail('unbound_import_data_access', symbol=trap_names.get(address & ~63, 'unknown'),
             address=hex(address), width=size)

    u.hook_add(UC_HOOK_CODE, code_hook)
    u.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, token_access,
               begin=trap_base, end=trap_base+0xffff)
    try:
        ext_offset, ext_count = dys[14:16]
        records = allocate(ext_count*8)
        u.mem_write(records, data[ext_offset:ext_offset+ext_count*8])
        error = allocate(512)
        result = native('im2_apply_external_relocs', [records, ext_count, nsyms,
                        map_callback, resolve_callback, 0, error, 512])
        if result:
            raise RuntimeError('Production external relocation planner rejected diagnostic bindings')
        report['external_relocations_applied'] = ext_count
        indirect_offset, _ = dys[12:14]
        patched = 0
        for va, size, reserved in indirect:
            for j in range(size//4):
                index = struct.unpack_from('<I', data, indirect_offset+4*(reserved+j))[0]
                if index & 0xc0000000:
                    continue  # local/absolute pointers retain original zero-slide values
                u.mem_write(va+j*4, struct.pack('<I', bindings[index]))
                patched += 1
        report['external_indirect_slots_applied'] = patched
        if args.register_metadata:
            phase = 'metadata'
            report['guest_classes_registered'] = 0
            report['guest_cfstrings_registered'] = 0
            references = []
            for name, segment, va, size in owner_sections:
                if name == '__objc_catlist' and size:
                    raise RuntimeError('Objective-C categories require actual method merging')
                if name not in ('__objc_classlist', '__objc_classrefs', '__objc_superrefs', '__cfstring'):
                    continue
                width = 16 if name == '__cfstring' else 4
                if segment != '__DATA' or size % width or not any(lo <= va and va+size <= hi for lo, hi in ranges):
                    raise RuntimeError('Invalid metadata section bounds')
                if name == '__objc_classlist':
                    result = native('im2_objc_register_guest_list', [va, size//4, metadata_read, 0, error, 512])
                    if result:
                        raise RuntimeError(bytes(u.mem_read(error, 512)).split(b'\0')[0].decode())
                    report['guest_classes_registered'] += size//4
                elif name in ('__objc_classrefs', '__objc_superrefs'):
                    references.append((va, size//4))
            for name, segment, va, size in owner_sections:
                if name == '__cfstring':
                    result = native('im2_foundation_register_cfstrings', [va, size//16, metadata_read, 0, error, 512])
                    if result:
                        raise RuntimeError(bytes(u.mem_read(error, 512)).split(b'\0')[0].decode())
                    report['guest_cfstrings_registered'] += size//16
            report['guest_reference_validation_errors'] = []
            for va, count in references:
                if native('im2_objc_validate_guest_refs', [va, count, metadata_read, 0, error, 512]):
                    report['guest_reference_validation_errors'].append(bytes(u.mem_read(error, 512)).split(b'\0')[0].decode())
            if report['guest_reference_validation_errors'] and not args.probe_main:
                raise RuntimeError('Guest class reference validation failed; see recorded errors')
        functions = allocate(len(initializer_slots)*4)
        for i, va in enumerate(initializer_slots):
            u.mem_write(functions+i*4, bytes(u.mem_read(va, 4)))
        text = next(s for s in segments if s[0] == '__TEXT')
        phase = 'constructors'
        result = native('im2_run_initializers', [functions, len(initializer_slots),
                        text[1], text[2], init_callback, 0])
        if result:
            raise RuntimeError('Production initializer validation failed')
        report['status'] = 'all_constructors_returned'
        report['registered_exit_callbacks'] = native('im2_exit_pending', [])
        report['exit_callbacks_entered'] = 0
        report['exit_callbacks_drained'] = False
        if args.probe_main:
            phase = 'main'
            candidates = [bindings[i] for i, name in enumerate(names) if name == '_main' and i in bindings]
            if len(candidates) != 1 or not any(name == '__text' and start <= (candidates[0] & ~1) < start+size
                                             for name, segment, start, size in owner_sections):
                raise RuntimeError('No unique valid original _main')
            argv = allocate(8)
            arg0 = allocate(16)
            u.mem_write(arg0, b'IronMan2\0')
            u.mem_write(argv, struct.pack('<II', arg0, 0))
            report['main_called'] = True
            report['main_result'] = native('call_guest', [candidates[0], 1, argv, 0, 0])
            report['status'] = 'main_returned'
        if args.drain_exit_callbacks:
            phase = 'exit_callbacks'
            native('darwin_run_exit_callbacks', [])
            report['pending_exit_callbacks_after_drain'] = native('im2_exit_pending', [])
            report['exit_callbacks_drained'] = report['pending_exit_callbacks_after_drain'] == 0
    except Exception as ex:
        report['status'] = 'stopped'
        report['failure'] = failure or {'reason': type(ex).__name__, 'detail': str(ex),
                                      'phase': phase,
                                      'pc': hex(u.reg_read(UC_ARM_REG_PC)),
                                      'lr': hex(u.reg_read(UC_ARM_REG_LR))}
    report['owner_code_executed_in_emulator'] = bool(report['constructors_entered'] or report['main_called'])
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k: report[k] for k in ('status', 'constructors_available',
                     'constructors_returned', 'main_called', 'physical_vita_verified')}))
    if 'failure' in report:
        print(json.dumps(report['failure']))


if __name__ == '__main__':
    main()
