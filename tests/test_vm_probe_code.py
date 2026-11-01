#!/usr/bin/env python3
"""CPU-only checks of actual C-generated diagnostic bytes, NOT Vita VM/cache tests."""
from pathlib import Path
import struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC

count=0
for patched in (0,1):
    data=Path(f'build/host-runtime/vm-own-code-{patched}.bin').read_bytes()
    base,=struct.unpack_from('<I',data)
    for offset,thumb,value in ((0,0,77 if patched else 42),(0x40,1,78 if patched else 43),(0x80,0,0x2468ace0 if patched else 0x13579bdf)):
        uc=Uc(UC_ARCH_ARM,UC_MODE_ARM)
        uc.mem_map(base,len(data)-4);uc.mem_write(base,data[4:])
        uc.mem_map(0x10000,0x2000);uc.reg_write(UC_ARM_REG_SP,0x11000);uc.reg_write(UC_ARM_REG_LR,0x10000)
        uc.emu_start((base+offset)|thumb,0x10000,count=20)
        assert uc.reg_read(UC_ARM_REG_PC)==0x10000
        assert uc.reg_read(UC_ARM_REG_R0)==value
        count+=1
print(f'PASS: {count} own ARM/Thumb return/layout calls in Unicorn. Native permissions/cache and physical Vita NOT tested.')
