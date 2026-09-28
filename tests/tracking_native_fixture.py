"""Execute the supplied client's unmodified minimap collector in an isolated x86 fixture.

Requires pefile and unicorn. Never launches, loads a DLL into, or attaches to WoW.
The object table, server resource mask and engine services are synthetic inputs;
the collector, trivial-quest predicate and resource-mask predicate are real code.
This proves native marker-list admission, not real rendering or server behavior.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import pefile
from check_tracking_client import verify as compatible
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP


class CollectorFixture:
    PLAYER = 0x2000000
    OBJECT = 0x2002000
    DESCRIPTOR = 0x2004000
    CONTEXT = 0x2006000
    LOCK = 0x2007000
    ROW = 0x2008000
    VTABLE = 0x2009000
    STUB = 0x2010000
    STACK = 0x202f000
    STOP = 0x202fff0

    def __init__(self, path):
        self.path = Path(path)
        pe = pefile.PE(str(path))
        assert pe.OPTIONAL_HEADER.ImageBase == 0x400000
        assert pe.FILE_HEADER.Machine == 0x14c
        self.cpu = Uc(UC_ARCH_X86, UC_MODE_32)
        size = (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095
        self.cpu.mem_map(0x400000, size)
        self.cpu.mem_write(0x400000, pe.get_memory_mapped_image())
        self.cpu.mem_map(self.PLAYER, 0x30000)
        self.handlers = {}
        self.categories = []
        self.cpu.hook_add(UC_HOOK_CODE, self.dispatch)
        self.stub(0x4d3790, lambda: self.result(1, edx=0))  # local player GUID
        self.stub(0x4d4db0, lambda: self.result(self.OBJECT))  # object lookup
        self.stub(0x70ba00, lambda: self.result(1))  # object is trackable
        self.stub(0x70ef30, lambda: self.result(self.LOCK))
        self.stub(0x6dca00, lambda: self.result(0, pop=4))  # unrelated unit tracking
        self.stub(0x77f090, lambda: self.result(0))  # no extra map layer
        self.stub(0x57f670, self.append)  # capture the native category's allocation
        self.stub(0x57c660, self.transform)  # projection output only
        self.stub(self.STUB, self.position)
        self.stub(self.STUB + 16, lambda: self.result(0))  # player can track
        self.cpu.mem_write(self.STUB + 32, bytes.fromhex('d9e8c3'))  # fld1; ret
        self.word(self.PLAYER, self.VTABLE)
        self.word(self.PLAYER + 8, self.PLAYER + 0x200)
        self.word(self.PLAYER + 0x200, 1)
        self.word(self.PLAYER + 0x1008, self.PLAYER + 0x100)
        self.word(self.VTABLE + 0x128, self.STUB + 16)
        self.word(self.OBJECT, self.VTABLE)
        self.word(self.OBJECT + 8, self.OBJECT + 0x200)
        self.word(self.OBJECT + 0xd0, self.DESCRIPTOR)
        self.word(self.VTABLE + 0x2c, self.STUB)
        self.word(self.CONTEXT, self.PLAYER)
        self.word(self.CONTEXT + 0x18, 0x42c80000)  # radius 100
        self.word(self.CONTEXT + 0x20, self.CONTEXT + 0x100)
        self.word(self.CONTEXT + 0x120, self.VTABLE)
        self.word(self.VTABLE + 0x28, self.STUB + 32)
        self.word(self.LOCK + 4, 2)  # lock skill requirement
        self.word(self.LOCK + 0x24, 2)  # herbalism bit 1

    def word(self, address, value):
        self.cpu.mem_write(address, struct.pack('<I', value))

    def read(self, address):
        return struct.unpack('<I', self.cpu.mem_read(address, 4))[0]

    def stub(self, address, handler):
        self.handlers[address] = handler
        self.cpu.mem_write(address, b'\xc3')

    def result(self, value, pop=0, edx=None):
        sp = self.cpu.reg_read(UC_X86_REG_ESP)
        self.cpu.reg_write(UC_X86_REG_EAX, value)
        if edx is not None:
            self.cpu.reg_write(UC_X86_REG_EDX, edx)
        self.cpu.reg_write(UC_X86_REG_EIP, self.read(sp))
        self.cpu.reg_write(UC_X86_REG_ESP, sp + 4 + pop)

    def dispatch(self, cpu, address, size, data):
        if address in self.handlers:
            self.handlers[address]()

    def position(self):
        dest = self.read(self.cpu.reg_read(UC_X86_REG_ESP) + 4)
        self.cpu.mem_write(dest, struct.pack('<3f', 1, 1, 0))
        self.result(dest, pop=4)

    def transform(self):
        dest = self.read(self.cpu.reg_read(UC_X86_REG_ESP) + 4)
        self.cpu.mem_write(dest, struct.pack('<2f', 0.1, 0.1))
        self.result(dest)

    def append(self):
        category = (self.cpu.reg_read(UC_X86_REG_ECX) - 0xbf8098) // 16
        self.categories.append(category)
        self.result(self.ROW)

    def collect(self, *, quests, herbs, quest_object, resource_mask=None, quest_status=2):
        self.categories.clear()
        self.word(0xbeba64, 0xa11d68 if quests else 0)
        self.word(0xbeba68, 2383 if herbs else 0)
        self.word(self.PLAYER + 0x100 + 0xdac, (2 if herbs else 0) if resource_mask is None else resource_mask)
        self.word(self.OBJECT + 0x208, 9 if quest_object else 0x21)
        self.word(self.OBJECT + 0x90, quest_status if quest_object else 0)
        self.word(self.DESCRIPTOR + 0x48, 1)
        self.cpu.mem_write(self.DESCRIPTOR + 0x2d, b'\x03')
        for i, value in enumerate((self.STOP, 2, 0, self.CONTEXT)):
            self.word(self.STACK + 4*i, value)
        self.cpu.reg_write(UC_X86_REG_ESP, self.STACK)
        self.cpu.emu_start(0x57f7f0, self.STOP, count=20000)
        assert self.cpu.reg_read(UC_X86_REG_EIP) == self.STOP, 'Collector did not return'
        return list(self.categories)


def verify(path):
    compatible(path)
    fixture = CollectorFixture(path)
    cases = []
    for quests, herbs in ((False, False), (True, False), (False, True), (True, True)):
        quest = fixture.collect(quests=quests, herbs=herbs, quest_object=True)
        herb = fixture.collect(quests=quests, herbs=herbs, quest_object=False)
        assert quest == ([9] if quests else []), (quests, herbs, quest)
        assert herb == ([8] if herbs else []), (quests, herbs, herb)
        cases.append(dict(quests=quests, herbs=herbs, questMarkerCategories=quest, herbMarkerCategories=herb))
    # A held spell ID is insufficient: native resource admission still requires
    # the server-fed mask. This guards against a false "two pointers means dots" proof.
    assert fixture.collect(quests=True, herbs=True, quest_object=False, resource_mask=0) == []
    assert fixture.collect(quests=True, herbs=True, quest_object=True, quest_status=4) == [11]
    assert fixture.collect(quests=False, herbs=True, quest_object=True, quest_status=4) == []
    return dict(client=str(Path(path).resolve()), sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest(),
                cases=cases, absentResourceMask='no herb marker despite tracked spell ID',
                trivialRepeatableQuest='category 11 only with service enabled',
                scope='Unmodified native collector; synthetic objects and server mask; no pixels or live server')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('client', type=Path, nargs='+')
    args = parser.parse_args()
    print(json.dumps([verify(path) for path in args.client], indent=2))
