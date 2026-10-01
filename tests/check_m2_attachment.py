"""Replay the compiled SDK wrapper and exact native attachment/detachment code.

All models and attachment tables are fixture memory. No game process, model file,
appearance, rendering or roster acceptance is established. Native list mutation,
reference ownership, missing-slot behavior and calling convention are exercised.
Requires pefile/unicorn; pass the separate Win32 fixture DLL, never WarcraftXL.dll.
"""
import argparse
import json
from pathlib import Path
import struct

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import (UC_X86_REG_ESP, UC_X86_REG_EIP,
    UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)


class AttachmentFixture:
    MEMORY, STACK, STOP = 0x20000000, 0x21000000, 0x22000000

    def __init__(self, client, library):
        self.cpu = Uc(UC_ARCH_X86, UC_MODE_32)
        self.methods = {}
        for path in (client, library):
            pe = pefile.PE(str(path), fast_load=True)
            if pe.FILE_HEADER.Machine != 0x14c:
                raise ValueError('Fixture inputs must be Win32 PE images')
            base = pe.OPTIONAL_HEADER.ImageBase
            self.cpu.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
            self.cpu.mem_write(base, pe.get_memory_mapped_image())
            if path == library:
                pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
                for symbol in pe.DIRECTORY_ENTRY_EXPORT.symbols:
                    if symbol.name:
                        self.methods[symbol.name.decode().lstrip('_')] = base + symbol.address
        if set(self.methods) != {'Attach', 'Detach'}:
            raise ValueError('Expected the separate compiled attachment fixture exports')
        self.cpu.mem_map(self.MEMORY, 0x10000)
        self.cpu.mem_map(self.STACK, 0x10000)
        self.cpu.mem_map(self.STOP, 0x1000)

    def put(self, address, value):
        self.cpu.mem_write(address, struct.pack('<I', value))

    def get(self, address):
        return struct.unpack('<I', self.cpu.mem_read(address, 4))[0]

    def model(self, offset, *, live=False, slots=()):
        model = self.MEMORY + offset
        self.put(model, 1)  # owner's reference: detachment must not destroy this instance
        self.put(model + 0x10, int(live))
        if live:
            shared, header, table = model + 0x400, model + 0x600, model + 0x900
            self.put(model + 0x2c, shared)
            self.put(shared + 0x150, header)
            self.put(header + 0xf8, len(slots))
            self.put(header + 0xfc, table)
            for index, value in enumerate(slots):
                self.cpu.mem_write(table + index * 2, struct.pack('<H', value))
        return model

    def call(self, method, *args):
        sp = self.STACK + 0xf000
        self.put(sp, self.STOP)
        for index, value in enumerate(args, 1):
            self.put(sp + index * 4, value)
        saved = {UC_X86_REG_EBX: 0xabc001, UC_X86_REG_ESI: 0xabc002,
                 UC_X86_REG_EDI: 0xabc003, UC_X86_REG_EBP: 0xabc004}
        for register, value in saved.items():
            self.cpu.reg_write(register, value)
        self.cpu.reg_write(UC_X86_REG_ESP, sp)
        self.cpu.emu_start(self.methods[method], self.STOP, count=10000)
        assert self.cpu.reg_read(UC_X86_REG_EIP) == self.STOP
        assert self.cpu.reg_read(UC_X86_REG_ESP) == sp + 4, 'SDK/native stack cleanup mismatch'
        for register, value in saved.items():
            assert self.cpu.reg_read(register) == value, 'SDK/native callee-saved register mismatch'

    def attached(self, child, parent, slot, point, force):
        assert self.get(child + 0x48) == parent
        assert self.get(child + 0x50) == slot
        assert self.get(child + 0x54) == point
        assert bool(self.get(child + 0x10) & 0x40000) == force
        assert self.get(child) == 2, 'Attachment must acquire exactly one child reference'

    def detached(self, child):
        assert self.get(child + 0x48) == 0
        assert self.get(child + 0x50) == 0xffffffff
        assert self.get(child + 0x5c) == self.get(child + 0x60) == 0
        assert self.get(child) == 1, 'Detachment must preserve the original owner reference'


def exercise(client, library):
    passed = []
    f = AttachmentFixture(client, library)
    parent = f.model(0, live=True, slots=(7, 0xffff))
    child = f.model(0x1000)
    f.call('Attach', child, parent, 0, 0)
    f.attached(child, parent, 0, 7, False)
    assert f.get(parent + 0x58) == child and f.get(child + 0x5c) == parent + 0x58
    f.call('Detach', parent, 0)
    f.detached(child)
    assert f.get(parent + 0x58) == 0
    passed.append('compiled-child-parent-order-and-balanced-owner-reference')

    for slot in (1, 9):  # explicit missing point and out-of-range slot
        f = AttachmentFixture(client, library)
        parent = f.model(0, live=True, slots=(7, 0xffff))
        child = f.model(0x1000)
        f.call('Attach', child, parent, slot, 0)
        assert f.get(child + 0x48) == f.get(parent + 0x58) == 0 and f.get(child) == 1
        f.call('Attach', child, parent, slot, 1)
        f.attached(child, parent, slot, 0xffff, True)
        f.call('Detach', parent, slot)
        f.detached(child)
    passed.append('missing-slot-rejected-or-explicitly-forced')

    f = AttachmentFixture(client, library)
    parent = f.model(0)  # native pending-parent path does not dereference the model header
    other = f.model(0x3000)
    a, b, c = (f.model(offset) for offset in (0x1000, 0x2000, 0x4000))
    for child, slot in ((a, 0), (b, 0), (c, 1)):
        f.call('Attach', child, parent, slot, 0)
        f.attached(child, parent, slot, 0xffff, False)
    assert f.get(parent + 0x58) == c and f.get(c + 0x60) == b and f.get(b + 0x60) == a
    f.call('Attach', a, other, 2, 0)  # native reparent releases the old attachment first
    f.attached(a, other, 2, 0xffff, False)
    assert f.get(b + 0x60) == 0 and f.get(other + 0x58) == a
    f.call('Detach', parent, 0)  # all matching siblings; the other slot must survive
    f.detached(b)
    f.attached(c, parent, 1, 0xffff, False)
    assert f.get(parent + 0x58) == c and f.get(c + 0x60) == 0
    f.call('Detach', parent, 1)
    f.call('Detach', other, 2)
    f.detached(c); f.detached(a)
    assert f.get(parent + 0x58) == f.get(other + 0x58) == 0
    passed.append('native-reparent-and-matching-sibling-detachment')
    return dict(status='offline-native-attachment-contract-passed', cases=passed,
                limits='Synthetic model state; no roster appearance, rendering, hooks or live acceptance.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = exercise(args.client, args.fixture)
    encoded = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding='utf-8')
    print(encoded)
