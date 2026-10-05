"""Execute an x86 hover-only continuation against pristine 12340 instructions.

Frame selection, eligibility, GUID publication and return are native. Geometry,
object residency and cursor/UI leaves are synthetic. No process, installed DLL,
client writes or rendered performance measurement is involved.
"""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import struct
import sys


def exercise(client, library, case):
    import pefile
    from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
        UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
        UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW)
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.reg_write(UC_X86_REG_FPCW, 0x37f)
    exports = {}
    for path in (client, library):
        pe = pefile.PE(str(path))
        assert pe.FILE_HEADER.Machine == 0x14c
        base = pe.OPTIONAL_HEADER.ImageBase
        cpu.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
        cpu.mem_write(base, pe.get_memory_mapped_image())
        if path == library:
            for symbol in pe.DIRECTORY_ENTRY_EXPORT.symbols:
                if symbol.name:
                    exports[symbol.name.decode().lstrip('_')] = base + symbol.address
    memory, stack, stop, trampoline = 0x20000000, 0x21000000, 0x22000000, 0x23000000
    for address, length in ((memory, 0x100000), (stack, 0x10000), (stop, 0x1000),
                            (trampoline, 0x1000)):
        cpu.mem_map(address, length)
    frame, input_, obj, player, header, desc, vtable = (
        memory + value for value in (0x1000, 0x4000, 0x8000, 0x10000, 0x20000, 0x21000, 0x23000))
    controls, virtual_leaf, ui_frame = memory + 0x24000, memory + 0x25000, memory + 0x27000
    events = []
    new_guid, old_guid, player_guid = 0x11110001, 0x11110002, 0xaaaa0001

    def put(address, value): cpu.mem_write(address, struct.pack('<I', value & 0xffffffff))
    def get(address): return struct.unpack('<I', cpu.mem_read(address, 4))[0]
    def arg(index): return get(cpu.reg_read(UC_X86_REG_ESP) + index * 4)
    def ret(value=0, cleanup=0, high=0):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        cpu.reg_write(UC_X86_REG_EAX, value)
        cpu.reg_write(UC_X86_REG_EDX, high)
        cpu.reg_write(UC_X86_REG_EIP, get(sp))
        cpu.reg_write(UC_X86_REG_ESP, sp + 4 + cleanup)
    def call(address, *args, this=0, stack_offset=0xf000):
        sp = stack + stack_offset
        put(sp, stop)
        for index, value in enumerate(args, 1): put(sp + index * 4, value)
        cpu.reg_write(UC_X86_REG_ESP, sp)
        cpu.reg_write(UC_X86_REG_ECX, this)
        try:
            cpu.emu_start(address, stop, count=100000)
        except UcError as error:
            raise RuntimeError(f"{case['name']}: native fault at "
                f"{cpu.reg_read(UC_X86_REG_EIP):08x}, ESP={cpu.reg_read(UC_X86_REG_ESP):08x}, "
                f"events={events}") from error
        assert cpu.reg_read(UC_X86_REG_EIP) == stop, (case, events)
        return sp

    # MinHook-style tail trampoline: execute the untouched seven overwritten
    # instructions' bytes, then return to the original native suffix.
    overwritten = bytes(cpu.mem_read(0x4fa32e, 7))
    assert overwritten == bytes.fromhex('d9 45 08 51 d9 1c 24')
    cpu.mem_write(trampoline, overwritten + b'\xe9' +
                  struct.pack('<i', 0x4fa335 - (trampoline + 12)))
    cpu.mem_write(0x4fa32e, b'\xe9' + struct.pack('<i', exports['TailHook'] - 0x4fa333) + b'\x90\x90')
    call(exports['SetOriginalTail'], trampoline)

    put(frame + 0xa0, 0 if case.get('no_input') else input_)
    put(input_ + 0x78, 0 if case.get('input_lost') else (
        ui_frame if case.get('ui_owner') else frame))
    put(ui_frame + 4, 1)
    put(ui_frame + 8, 1)
    put(ui_frame + 0x60, 0x10000 if case.get('ui_unit', True) else 0)
    put(frame + 0x2c8, old_guid)
    put(frame + 0x31c, 2 if case.get('suppressed') else 0)
    put(0xb7436c, frame)
    put(0xbd07a0, old_guid)
    put(0xbd07b0, new_guid)  # synthetic UI handler returns the native target unit token
    cpu.mem_write(input_ + 0x1224, struct.pack('<ff', 0.4, 0.6))
    put(obj, vtable)
    put(obj + 8, header)
    put(obj + 0xd0, desc)
    put(header, new_guid)
    put(header + 8, 0x200)  # unknown object kind: native cursor fallback still publishes
    put(vtable + 0xa4, virtual_leaf)
    cpu.mem_write(desc + 0x2d, bytes([0x21 if case.get('special_kind', True) else 0]))
    put(player + 0xb8, desc + 0x100)
    put(0xac80a8, 0)

    def dispatch(_, address, size, data):
        if address == 0x490770:
            events.append('base-frame-update')
            ret(cleanup=4)
        elif address == 0x56d050:
            events.append('frame-prefix')
            ret()
        elif address == 0x47bff0:
            events.append('cursor-conversion')
            cpu.mem_write(arg(3), struct.pack('<f', 100.0))
            cpu.mem_write(arg(4), struct.pack('<f', 200.0))
            ret()
        elif address in (0x7fd620, 0x7fd750, 0x7fd650): ret()
        elif address == 0x5f95d0: ret(controls)
        elif address == 0x715c30: ret()  # no hovered nameplate
        elif address == 0x4f9da0:
            events.append('fresh-pick')
            assert cpu.reg_read(UC_X86_REG_ECX) == frame and arg(3) == 1
            cpu.mem_write(arg(4), struct.pack('<II10f', new_guid, 0, *([1.0] * 10)))
            ret(case['type'], cleanup=16)
        elif address == 0x4d3790: ret(player_guid)
        elif address == 0x4d4db0:
            guid = arg(1)
            ret(player if guid == player_guid else (
                obj if guid == new_guid and not case.get('disappeared') else 0))
        elif address == 0x4038f0: ret(player)
        elif address == 0x7819c0: ret(new_guid if case.get('owner_matches') else 0)
        elif address == virtual_leaf: ret(1)
        elif address == 0x616800: events.append('cursor-shape'); ret()
        elif address == 0x616920: events.append('cursor-reset'); ret()
        elif address == 0x61b290: ret()
        elif address in (0x743bc0, 0x743c70): ret(cleanup=4)
        elif address in (0x4f66c0, 0x4f8190):
            events.append(f'native-helper-{address:08x}')  # execute native helper body
        elif address == 0x4f5980:
            events.append('native-setter')  # execute native setter/publisher bodies
        elif address == 0x51f838:
            # The GUID writes precede this native object lookup. Model a nested
            # action at the first post-publication callback boundary; run its
            # cached-GUID selection read rather than supplying an invented hit.
            events.append('published-before-nested-action')
            assert get(0xbd07a0) == expected_guid, (case, hex(get(0xbd07a0)))
            saved = cpu.context_save()
            nested_sp = call(0x527f00, controls, stack_offset=0xe000)
            assert cpu.reg_read(UC_X86_REG_ESP) == nested_sp + 4
            cpu.context_restore(saved)
        elif address == 0x84df60: ret(1)  # InteractUnit's Lua argument is a string
        elif address == 0x84e0e0: ret(0xa02e6c if arg(2) == 1 else 0x9fc4a0)
        elif address == 0x76e780:
            left = bytes(cpu.mem_read(arg(1), 32)).split(b'\0', 1)[0]
            right = bytes(cpu.mem_read(arg(2), 32)).split(b'\0', 1)[0]
            count = arg(3)
            ret(0 if left[:count].lower() == right[:count].lower() else 1, cleanup=12)
        elif address == 0x5277b0:
            events.append('nested-native-interact-selection')
            assert arg(1) == expected_guid and arg(2) == 0, (case, hex(arg(1)))
            ret(1)
        elif address in (0x84e2a0, 0x84e670, 0x84e350, 0x84e600, 0x84dbf0): ret()
        elif address == 0x817db0: ret(controls)
        elif address == 0x8192f0: ret(1)
        elif address == 0x84ec50:
            events.append('ordinary-native-ui-query')
            ret()
        elif address == 0x4fa368:
            events.append('native-epilogue')
        elif address == 0x5fab70:
            events.append('frame-suffix')
            ret(cleanup=4)
        elif address in (0x51fde0, 0x51f5c0, 0x524010): ret()
        elif address == 0x56ded0: ret()

    expected_guid = new_guid if (case['type'] == 2 or case['type'] == 3 and
        case.get('special_kind', True) and not case.get('owner_matches')) else 0
    if any(case.get(key) for key in ('suppressed', 'disappeared')):
        expected_guid = 0
    if any(case.get(key) for key in ('no_frame', 'no_input', 'input_lost')):
        expected_guid = old_guid  # native retains UI-owned state; our world cache must expire
    if case.get('ui_owner'):
        expected_guid = new_guid if case.get('ordinary') and case.get('ui_unit', True) else old_guid
    cpu.hook_add(UC_HOOK_CODE, dispatch)
    sentinels = {UC_X86_REG_EBX: 0x13572468, UC_X86_REG_ESI: 0x24681357,
                 UC_X86_REG_EDI: 0x31415926, UC_X86_REG_EBP: 0x27182818}
    for register, value in sentinels.items(): cpu.reg_write(register, value)
    if case.get('ordinary'):
        sp = call(0x4fa040, 0, this=frame)
        assert cpu.reg_read(UC_X86_REG_ESP) == sp + 8
        assert 'base-frame-update' in events and 'frame-suffix' in events
        if case.get('ui_owner'):
            before = list(events)
            sp = call(exports['Refresh'], frame)
            assert cpu.reg_read(UC_X86_REG_ESP) == sp + 4
            assert events == before, 'supplementary world refresh must not replay native UI Lua'
    else:
        sp = call(exports['Refresh'], 0 if case.get('no_frame') else frame)
        assert cpu.reg_read(UC_X86_REG_ESP) == sp + 4
        assert 'base-frame-update' not in events and 'frame-suffix' not in events
    for register, value in sentinels.items(): assert cpu.reg_read(register) == value, (case, register)
    assert get(0xbd07a0) == expected_guid, (case, hex(get(0xbd07a0)), events)
    return dict(case=case, guid=expected_guid, stackAndCalleeSaved=True, events=events)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--python-deps', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.python_deps: sys.path.insert(0, str(args.python_deps.resolve()))
    digest = hashlib.sha256(args.client.read_bytes()).hexdigest()
    assert digest == '1df8ba4be431b5396a27680e44d287c6e364b6ba34b97599deadb18541737ef6'
    assert args.client.resolve().parent not in args.output.resolve().parents
    cases = [dict(name=f'type-{type_}', type=type_) for type_ in range(4)]
    cases += [dict(name=key, type=2, **{key: True}) for key in
              ('input_lost', 'suppressed', 'disappeared', 'no_frame', 'no_input', 'ordinary')]
    cases += [dict(name='special-kind-rejected', type=3, special_kind=False),
              dict(name='special-owner-rejected', type=3, owner_matches=True),
              dict(name='ui-owner-guard', type=2, ui_owner=True),
              dict(name='ordinary-ui-unit', type=2, ui_owner=True, ordinary=True),
              dict(name='ordinary-ui-nonunit', type=2, ui_owner=True, ui_unit=False, ordinary=True)]
    report = dict(status='offline-native-hover-continuation', executableSha256=digest,
        dependencies={name: importlib.metadata.version(name) for name in ('pefile', 'unicorn')},
        cases=[exercise(args.client, args.fixture, case) for case in cases])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'hover continuation: {len(cases)} cases passed; {args.output}')


if __name__ == '__main__': main()
