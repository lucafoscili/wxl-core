"""Execute pristine 12340 request allocation/submission in an isolated x86 fixture.

Native request-pool reuse, format copy, texture gather and queue publication run.
Locks, signal and one texture-gather callback are synthetic. This checks the two
new hook seams' original ABI/order, not installed hooks or character rendering.
Requires pefile and unicorn; never launches, attaches to or changes WoW.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def exercise(pe, texture_format, dirty):
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
                                  UC_X86_REG_EDI, UC_X86_REG_EIP, UC_X86_REG_ESI,
                                  UC_X86_REG_ESP)

    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0x400000, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    cpu.mem_write(0x400000, pe.get_memory_mapped_image())
    cpu.mem_map(0x2000000, 0x10000)
    component, request, head, callback = 0x2000000, 0x2001000, 0x2002000, 0x2003000
    stack, stop = 0x200F000, 0x200FFF0
    events = []

    def word(address, value): cpu.mem_write(address, struct.pack('<I', value))
    def read(address): return struct.unpack('<I', cpu.mem_read(address, 4))[0]
    def returned():
        esp = cpu.reg_read(UC_X86_REG_ESP)
        cpu.reg_write(UC_X86_REG_EIP, read(esp))
        cpu.reg_write(UC_X86_REG_ESP, esp + 4)

    word(component + 0x14, texture_format)
    word(component + 0xC, dirty)
    word(0xAC4710, request)  # one reusable native request
    word(request, 3)        # live/completed flags, reset by native allocation
    word(request + 4, 99)
    word(request + 8, 99)
    word(request + 0x34, 99)
    word(0xAC46F0, 0x100)  # queue node offset
    word(0xAC46F4, head)
    word(head + 4, head)
    word(0xB6B4B8, callback)
    word(stack, stop)
    cpu.reg_write(UC_X86_REG_ESP, stack)
    cpu.reg_write(UC_X86_REG_ECX, component)
    sentinels = {UC_X86_REG_ESI: 0x12345678, UC_X86_REG_EDI: 0x23456789,
                 UC_X86_REG_EBX: 0x34567890}
    for register, value in sentinels.items(): cpu.reg_write(register, value)

    def dispatch(_, address, size, data):
        if address == 0x4F10E0:
            events.append('allocate')  # execute original allocation, including its ret
        elif address == 0x4F1799:
            assert cpu.reg_read(UC_X86_REG_EAX) == request
            assert read(request) == 1 and read(request + 8) == 0 and read(request + 0x34) == 0
            events.append('allocated-before-format-copy')
        elif address == callback:
            assert cpu.reg_read(UC_X86_REG_ECX) == component
            assert read(component + 0x52C) == request and read(request + 4) == texture_format
            word(request + 0xC, 0xABCDEF)
            events.append('gather')
            returned()
        elif address == 0x774640:
            assert read(request + 4) == texture_format
            assert bool(read(request + 0xC)) == bool(dirty)
            events.append('queue-lock')
            returned()
        elif address == 0x774650:
            assert read(0xAC46F4) == request + 0x100
            events.append('published')
            returned()
        elif address == 0x774720:
            events.append('signal')
            returned()

    cpu.hook_add(UC_HOOK_CODE, dispatch)
    cpu.emu_start(0x4F1790, stop, count=10000)
    assert cpu.reg_read(UC_X86_REG_EIP) == stop
    assert cpu.reg_read(UC_X86_REG_ESP) == stack + 4, 'thiscall takes no stack arguments'
    for register, value in sentinels.items(): assert cpu.reg_read(register) == value
    assert events == ['allocate', 'allocated-before-format-copy'] + (
        ['gather'] if dirty else []) + ['queue-lock', 'published', 'signal']
    return dict(format=texture_format, dirty=dirty, events=events, stackAndCalleeSaved=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', type=Path, required=True, help='pristine 12340 Wow.exe')
    parser.add_argument('--python-deps', type=Path, help='existing pefile/unicorn package directory')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.python_deps: sys.path.insert(0, str(args.python_deps.resolve()))
    import pefile
    pe = pefile.PE(str(args.client))
    assert pe.OPTIONAL_HEADER.ImageBase == 0x400000 and pe.FILE_HEADER.Machine == 0x14C
    output = args.output.resolve()
    assert args.client.resolve().parent not in output.parents, 'output must stay outside the client'
    cases = [exercise(pe, texture_format, dirty) for texture_format in (2, 6) for dirty in (0, 1)]
    report = dict(status='offline-native-seams-pass-not-rendering',
                  executableSha256=hashlib.sha256(args.client.read_bytes()).hexdigest(), cases=cases)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'skin-alpha request seams: {len(cases)} cases passed; {output}')


if __name__ == '__main__': main()
