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
        UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW, UC_X86_REG_GDTR,
        UC_X86_REG_FS, UC_X86_REG_CS, UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS)
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
            tls = pe.DIRECTORY_ENTRY_TLS.struct
    memory, stack, stop, trampoline = 0x20000000, 0x21000000, 0x22000000, 0x23000000
    for address, length in ((memory, 0x100000), (stack, 0x10000), (stop, 0x1000),
                            (trampoline, 0x1000)):
        cpu.mem_map(address, length)
    frame, input_, obj, player, header, desc, vtable = (
        memory + value for value in (0x1000, 0x4000, 0x8000, 0x10000, 0x20000, 0x21000, 0x23000))
    controls, virtual_leaf, ui_frame = memory + 0x24000, memory + 0x25000, memory + 0x27000
    events = []
    new_guid, old_guid, player_guid = 0x11110001, 0x11110002, 0xaaaa0001
    ray_guid = new_guid
    resident_guids = {new_guid, old_guid, new_guid + 2}
    targeting = 0
    policy = case.get('policy')
    production_hooks = bool(policy and not case.get('baseline'))
    inject_nested = not policy
    presentation_depth = 0
    action_entry = None

    def put(address, value): cpu.mem_write(address, struct.pack('<I', value & 0xffffffff))
    def get(address): return struct.unpack('<I', cpu.mem_read(address, 4))[0]
    # Same emulated x86 TLS convention as the existing resident fixture. The
    # production action-refresh reentrancy flag is thread-local, zero-initialized.
    gdt, teb, tls_block = 0x24000000, 0x25000000, 0x26000000
    for address in (gdt, teb, tls_block): cpu.mem_map(address, 0x4000)
    for index, access in ((1, 0x9b), (2, 0x93)):
        descriptor = 0xffff | (access << 40) | (0xf << 48) | (0xc << 52)
        cpu.mem_write(gdt + index * 8, struct.pack('<Q', descriptor))
    descriptor = (0xffff | ((teb & 0xffffff) << 16) | (0x93 << 40) |
                  (0xf << 48) | (0xc << 52) | ((teb >> 24) << 56))
    cpu.mem_write(gdt + 24, struct.pack('<Q', descriptor))
    cpu.reg_write(UC_X86_REG_GDTR, (0, gdt, 0x1000, 0))
    cpu.reg_write(UC_X86_REG_CS, 8)
    for register in (UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS): cpu.reg_write(register, 16)
    cpu.reg_write(UC_X86_REG_FS, 24)
    put(tls.AddressOfIndex, 0)
    cpu.mem_write(tls_block, bytes(cpu.mem_read(tls.StartAddressOfRawData,
                                             tls.EndAddressOfRawData-tls.StartAddressOfRawData)))
    put(teb + 0x2c, teb + 0x1000)
    put(teb + 0x1000, tls_block)
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
    pick_leaf = memory + 0x28000
    if production_hooks:
        def hook(address, length, getter, setter, original):
            saved = bytes(cpu.mem_read(address, length))
            cpu.mem_write(original, saved + b'\xe9' + struct.pack('<i',
                address + length - (original + length + 5)))
            call(exports[setter], original)
            call(exports[getter])
            target = cpu.reg_read(UC_X86_REG_EAX)
            cpu.mem_write(address, b'\xe9' + struct.pack('<i', target-address-5)
                          + b'\x90' * (length-5))
        call(exports['SetOriginalPick'], pick_leaf)
        call(exports['PickAddress'])
        cpu.mem_write(0x4f9da0, b'\xe9' + struct.pack('<i',
                      cpu.reg_read(UC_X86_REG_EAX)-0x4f9da5))
        hook(0x4fa040, 6, 'FrameAddress', 'SetOriginalFrame', memory+0x28100)
        hook(0x60abf0, 5, 'ResolveAddress', 'SetOriginalResolve', memory+0x28200)
        hook(0x527f00, 7, 'InteractAddress', 'SetOriginalInteract', memory+0x28300)
        hook(0x4f5980, 6, 'PublicationAddress', 'SetOriginalPublication', memory+0x28400)
        hook(0x564db0, 9, 'MacroAddress', 'SetOriginalMacro', memory+0x28500)

    put(frame + 0xa0, 0 if case.get('no_input') else input_)
    put(input_ + 0x78, 0 if case.get('input_lost') else (
        ui_frame if case.get('ui_owner') else frame))
    put(ui_frame + 4, 1)
    put(ui_frame + 8, 1)
    put(ui_frame + 0x60, 0x10000 if case.get('ui_unit', True) else 0)
    put(frame + 0x2c8, old_guid)
    put(frame + 0x31c, 2 if case.get('suppressed') else 0)
    put(0xb7436c, 0 if case.get('no_frame') else frame)
    put(0xadfbc8, 0)  # fixture is an entered world, not the executable's initial load gate
    put(0xbd07a0, old_guid)
    put(0xbd07b0, new_guid)  # synthetic UI handler returns the native target unit token
    cpu.mem_write(input_ + 0x1224, struct.pack('<ff', 0.4, 0.6))
    put(obj, vtable)
    put(obj + 8, header)
    put(obj + 0xd0, desc)
    put(header, new_guid)
    put(header + 8, case.get('object_kind', 0x200))
    put(vtable + 0xa4, virtual_leaf)
    cpu.mem_write(desc + 0x2d, bytes([0x21 if case.get('special_kind', True) else 0]))
    put(player + 0xb8, desc + 0x100)
    put(0xac80a8, 0)
    spell_text = memory+0x30000
    cpu.mem_write(spell_text, b'!Fixture\0')
    put(0xbe8d98, 1); put(0xbe6d88, 123)  # one synthetic known spell, no spell-table execution
    cpu.mem_write(memory+0x31160,b'/cast [@mouseover] !Fixture\0')

    def dispatch(_, address, size, data):
        nonlocal presentation_depth
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
        elif address in (0x7fd620, 0x7fd750, 0x7fd650):
            ret(targeting if address == 0x7fd620 else 0)
        elif address == 0x5f95d0: ret(controls)
        elif address == 0x715c30: ret()  # no hovered nameplate
        elif address == (pick_leaf if production_hooks else 0x4f9da0):
            events.append('fresh-pick')
            assert cpu.reg_read(UC_X86_REG_ECX) == frame
            cpu.mem_write(arg(4), struct.pack('<II10f', ray_guid, 0, *([1.0] * 10)))
            put(header, ray_guid)
            ret(case['type'], cleanup=16)
        elif address == 0x4d3790: ret(player_guid)
        elif address == 0x4d4db0:
            guid = arg(1)
            ret(player if guid == player_guid else (
                obj if guid in resident_guids and not case.get('disappeared') else 0))
        elif address == 0x4038f0: ret(player)
        elif address == 0x7819c0: ret(new_guid if case.get('owner_matches') else 0)
        elif address == virtual_leaf: ret(1)
        elif address == 0x616800: events.append('cursor-shape'); ret()
        elif address == 0x616920: events.append('cursor-reset'); ret()
        elif address == 0x61b290: ret()
        elif address == 0x4f7a50:
            events.append('native-unit-cursor-helper')  # execute real unit helper
        elif address in (0x6d7aa0, 0x6dd060): ret()  # inactive unit-cursor special states
        elif address == 0x4883b0: ret(input_)
        elif address == 0x5143f0:
            events.append('unit-presentation-reset'); ret()
        elif address == 0x621070:
            events.append('unit-presentation-selection'); ret(cleanup=16)
        elif address == 0x81ac90:
            # Execute the native 81B530 event wrapper; only its Lua-dispatch leaf
            # is synthetic. A read here models an ordinary presentation callback,
            # not an action. The native frame GUID has not been committed yet.
            assert arg(1) == 0x142 and arg(2) == 0
            events.append('native-unit-event-142')
            if case.get('presentation_callback'):
                assert get(0xbd07a0) == new_guid
                assert get(frame+0x2c8) == old_guid
                saved = cpu.context_save()
                nested_offset = 0xd000 - presentation_depth*0x1000
                presentation_depth += 1
                call(0x60abf0, 0xa02e6c, memory+0x29000, 0, stack_offset=nested_offset)
                for key,entry in (('publication_action',0x527f00),('publication_macro',0x566e80)):
                    if case.get(key):
                        preserved={r:cpu.reg_read(r) for r in
                            (UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP)}
                        nested_sp=call(entry,controls,stack_offset=nested_offset)
                        assert cpu.reg_read(UC_X86_REG_ESP)==nested_sp+4
                        for r,value in preserved.items(): assert cpu.reg_read(r)==value
                presentation_depth -= 1
                cpu.context_restore(saved)
            ret()
        elif address in (0x514080, 0x514050): ret(cleanup=4)
        elif address == 0x4c74a0: ret()
        elif address in (0x743bc0, 0x743c70): ret(cleanup=4)
        elif address in (0x4f66c0, 0x4f8190):
            events.append(f'native-helper-{address:08x}')  # execute native helper body
        elif address == 0x4f5980:
            events.append('native-setter')  # execute native setter/publisher bodies
        elif address == 0x51f838 and inject_nested:
            # The GUID writes precede this native object lookup. Model a nested
            # action at the first post-publication callback boundary; run its
            # cached-GUID selection read rather than supplying an invented hit.
            events.append('published-before-nested-action')
            assert get(0xbd07a0) == expected_guid, (case, hex(get(0xbd07a0)))
            saved = cpu.context_save()
            nested_sp = call(0x527f00, controls, stack_offset=0xe000)
            assert cpu.reg_read(UC_X86_REG_ESP) == nested_sp + 4
            cpu.context_restore(saved)
        elif address in (0x84df60, 0x84df20): ret(1)  # synthetic valid Lua argument
        elif address == 0x84e0e0:
            if 0x527f00 <= arg(0) < 0x528000:
                ret(0xa02e6c if arg(2)==1 else 0x9fc4a0)
            elif action_entry in (0x540310, 0x566400):
                ret(spell_text if arg(2)==1 else 0xa02e6c)
            elif action_entry in (0x53e060, 0x5ac000): ret(0xa02e6c)
            else: ret(0xa02e6c if arg(2) == 1 else 0x9fc4a0)
        elif address in (0x84e070, 0x84e030): ret(123 if address==0x84e070 else 1)
        elif address == 0x88b9c0: ret(1)
        elif address == 0x540200: put(arg(2), 0); ret(123)
        elif address == 0x80da40:
            events.append('native-spell-target-consumed')
            assert arg(3)==expected_guid and arg(4)==0, (case, events, hex(arg(3)))
            ret()
        elif address == 0x5abbc0:
            events.append('native-action-target-consumed')
            assert get(arg(2))==expected_guid and get(arg(2)+4)==0
            ret()
        elif address == 0x565ba0:
            cpu.mem_write(cpu.reg_read(UC_X86_REG_ECX)+0x160,b'/cast [@mouseover] !Fixture\0')
            ret()
        elif address == 0x565b30: ret()
        elif address == 0x76ed20: ret(cleanup=12)  # synthetic bounded macro text copy
        elif address == 0x566df0: ret(memory+0x31000)  # synthetic selected macro
        elif address == 0x6f6020: ret(memory+0x31000,cleanup=8)  # macro-slot lookup leaf
        elif address == 0x5191c0: ret(1)  # synthetic macro permission leaf
        elif address == 0x76f1e0:
            # The native macro executor owns the loop and line dispatch. Text
            # splitting is synthetic and supplies one line before ending it.
            cpu.mem_write(arg(2),b'/cast [@mouseover] !Fixture\0')
            put(arg(1),0)
            ret(cleanup=20)
        elif address == 0x81b530 and arg(1)==0x17f:
            events.append('native-macro-line-dispatch')
            assert get(0xbd07a0)==expected_guid, (case,events)
            saved=cpu.context_save()
            call(0x60abf0,0xa02e6c,memory+0x29000,0,stack_offset=0xc000)
            assert get(memory+0x29000)==expected_guid
            cpu.context_restore(saved)
            ret()
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
    if policy:
        checks = []
        def fresh_count(): return events.count('fresh-pick')
        def cached():
            call(exports['Cached'])
            return cpu.reg_read(UC_X86_REG_EAX)
        def tick(time, guid, picks):
            nonlocal expected_guid
            expected_guid = guid
            call(exports['SetTime'], time)
            sp = call(0x4fa040, 0, this=frame)
            assert cpu.reg_read(UC_X86_REG_ESP) == sp+8
            for register, value in sentinels.items():
                assert cpu.reg_read(register) == value, (policy, register)
            assert get(0xbd07a0) == guid, (policy, time, hex(get(0xbd07a0)), events)
            assert fresh_count() == picks, (policy, time, fresh_count(), picks)
            checks.append(dict(time=time, guid=guid, freshPicks=picks, cached=bool(cached())))
        if policy == 'presentation-amplification':
            queries = case['queries_per_frame']
            for i in range(100):
                call(exports['SetTime'], i*10)
                call(0x4fa040, 0, this=frame)
                for _ in range(queries):
                    call(0x60abf0, 0xa02e6c, memory+0x29000, 0)
            assert fresh_count() == (100 if case.get('baseline') else 10)
            dispatches = 100
            assert events.count('native-helper-004f8190') == dispatches
            assert events.count('cursor-reset') == dispatches
            checks.append(dict(frames=100, presentationQueries=100*queries,
                freshPicks=fresh_count(), hoverDispatches=events.count('native-helper-004f8190'),
                cursorResets=events.count('cursor-reset')))
        elif policy == 'unit-publication-callback':
            expected_picks = 1
            tick(0, new_guid, expected_picks)
            publications = events.count('native-unit-event-142')
            assert publications == expected_picks, (case, events)
            assert events.count('unit-presentation-selection') == publications
            assert events.count('native-unit-cursor-helper') == publications
            checks.append(dict(freshPicks=fresh_count(), unitEvents=publications,
                unitPresentationSelections=publications,
                frameGuidCommittedAfterCallback=get(frame+0x2c8)==new_guid))
        elif policy == 'interval-wrap':
            tick(0, new_guid, 1)
            ray_guid = new_guid+2
            tick(16, new_guid, 1); tick(99, new_guid, 1); tick(100, ray_guid, 2)
            call(exports['Invalidate'])
            tick(0xfffffff0, ray_guid, 3)
            ray_guid = new_guid
            tick(0x40, new_guid+2, 3); tick(0x54, new_guid, 4)
        elif policy in ('interact-action', 'spell-name-action', 'spell-id-action', 'use-action',
                        'macro-action', 'macro-text-action', 'macro-slot-action', 'mouse-button'):
            tick(0, new_guid, 1)
            ray_guid = new_guid+2
            expected_guid = ray_guid
            inject_nested = True
            call(exports['SetTime'], 20)
            if policy == 'interact-action': call(0x527f00, controls)
            elif policy not in ('mouse-button',):
                action_entry={'spell-name-action':0x540310,'spell-id-action':0x53e060,
                              'use-action':0x5ac000,'macro-action':0x566e80,
                              'macro-text-action':0x566400,'macro-slot-action':0x566dc0}[policy]
                sp=call(action_entry, *( (1,0) if policy=='macro-slot-action' else (controls,) ))
                assert cpu.reg_read(UC_X86_REG_ESP)==sp+4
                for register,value in sentinels.items(): assert cpu.reg_read(register)==value
                if policy.startswith('macro-'):
                    assert 'native-macro-line-dispatch' in events
                elif policy.startswith('spell-'):
                    assert 'native-spell-target-consumed' in events
                else:
                    assert 'native-action-target-consumed' in events
                action_entry=None
            else: call(exports['BeforeInput'], 0x201)
            assert get(0xbd07a0) == ray_guid and fresh_count() == 2
            assert cached(), 'fresh action result should seed passive presentation'
            assert 'nested-native-interact-selection' in events
            inject_nested = False
            tick(21, ray_guid, 2)
            # Another action at the same timestamp still bypasses retained values.
            expected_guid = ray_guid
            call(exports['BeforeInput'], 0x201)
            assert fresh_count() == 3
        elif policy == 'unknown-callers':
            tick(0, new_guid, 1)
            ray_guid = new_guid+2
            for mode in (0, 1):
                sp = call(0x4f9da0, 0, 0, mode, memory+0x29000, this=frame)
                assert cpu.reg_read(UC_X86_REG_ESP) == sp+20
                assert get(memory+0x29000) == ray_guid
            assert fresh_count() == 3
        elif policy == 'miss-stays-fresh':
            for i in range(3):
                tick(i*10, 0, i+1)
                assert not cached()
        elif policy == 'cursor-and-focus':
            tick(0, new_guid, 1)
            ray_guid = new_guid+2
            call(exports['BeforeInput'], 0x200)
            tick(20, new_guid, 1)  # movement accepts the bounded visual age
            call(exports['BeforeInput'], 0x8)
            assert not cached()
            tick(21, ray_guid, 2)
        elif policy in ('input-loss', 'ui-transition', 'context-loss', 'load-loss',
                        'suppression-loss', 'target-mode', 'target-disappearance',
                        'external-guid', 'world-event', 'key-input'):
            tick(0, new_guid, 1)
            ray_guid = new_guid+2
            if policy == 'input-loss':
                put(input_+0x78, 0); tick(20, new_guid, 1); assert not cached()
                put(input_+0x78, frame)
            elif policy == 'ui-transition':
                put(input_+0x78, ui_frame); tick(20, new_guid, 1); assert not cached()
                before = list(events); call(exports['Refresh'], frame)
                assert events == before, 'no supplemental UI query'
                put(input_+0x78, frame)
            elif policy == 'context-loss':
                replacement = memory+0x40000
                cpu.mem_write(replacement, bytes(cpu.mem_read(input_, 0x1300)))
                put(frame+0xa0, replacement)
            elif policy == 'load-loss':
                put(0xadfbc8, 1); call(exports['Refresh'], frame); assert not cached()
                put(0xadfbc8, 0)
            elif policy == 'suppression-loss':
                put(frame+0x31c, 2); tick(20, 0, 2); assert not cached()
                put(frame+0x31c, 0)
            elif policy == 'target-mode':
                targeting = 1; tick(20, ray_guid, 2); assert not cached()
                targeting = 0
            elif policy == 'target-disappearance': resident_guids.remove(new_guid)
            elif policy == 'external-guid': put(0xbd07a0, old_guid)
            elif policy == 'world-event': call(exports['Invalidate'])
            else: call(exports['BeforeInput'], 0x100)
            tick(21, ray_guid, 3 if policy in ('target-mode', 'suppression-loss') else 2)
        elif policy in ('passive-cadence', 'mouseover-polling'):
            for i in range(100):
                call(exports['SetTime'], i*10)
                call(0x4fa040, 0, this=frame)
                if policy == 'mouseover-polling':
                    call(0x60abf0, 0xa02e6c, memory+0x29000, 0)
            assert fresh_count() == 10
            checks.append(dict(frames=100, freshPicks=fresh_count(),
                               unthrottledPassivePicks=100))
        else: raise AssertionError(policy)
        return dict(case=case, productionPolicy=True, stackAndCalleeSaved=True,
                    checks=checks, freshPicks=fresh_count(), events=events)
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
    import pefile
    digest = hashlib.sha256(args.client.read_bytes()).hexdigest()
    assert digest == '1df8ba4be431b5396a27680e44d287c6e364b6ba34b97599deadb18541737ef6'
    assert args.client.resolve().parent not in args.output.resolve().parents
    executable=pefile.PE(str(args.client))
    image=executable.get_memory_mapped_image()
    image_base=executable.OPTIONAL_HEADER.ImageBase
    def native_word(address): return struct.unpack_from('<I',image,address-image_base)[0]
    registrations=[('CastSpellByID',0xaccdf0,0x53e060),('CastSpellByName',0xaccde8,0x540310),
                   ('UseAction',0xacfa18,0x5ac000),('RunMacro',0xace1f8,0x566e80),
                   ('RunMacroText',0xace200,0x566400)]
    for name,table,function in registrations:
        offset=native_word(table)-image_base
        assert image[offset:offset+len(name)+1]==name.encode()+b'\0'
        assert native_word(table+4)==function
    native_calls=[(call,0x60abf0) for call in (0x53e0cb,0x54037b,0x5ac043)]
    native_calls += [(call,0x564db0) for call in (0x563336,0x56646d,0x566ddb,0x566e9f)]
    native_calls += [(0x5abdbc,0x566dc0)]
    for address,target in native_calls:
        assert image[address-image_base]==0xe8
        assert (address+5+native_word(address+1)) & 0xffffffff==target
    cases = [dict(name=f'type-{type_}', type=type_) for type_ in range(4)]
    cases += [dict(name=key, type=2, **{key: True}) for key in
              ('input_lost', 'suppressed', 'disappeared', 'no_frame', 'no_input', 'ordinary')]
    cases += [dict(name='special-kind-rejected', type=3, special_kind=False),
              dict(name='special-owner-rejected', type=3, owner_matches=True),
              dict(name='ui-owner-guard', type=2, ui_owner=True),
              dict(name='ordinary-ui-unit', type=2, ui_owner=True, ordinary=True),
              dict(name='ordinary-ui-nonunit', type=2, ui_owner=True, ui_unit=False, ordinary=True)]
    cases += [dict(name=name, policy=name, type=2) for name in (
        'interval-wrap', 'interact-action', 'spell-name-action', 'spell-id-action',
        'use-action', 'macro-action', 'macro-text-action', 'macro-slot-action', 'mouse-button',
        'unknown-callers', 'input-loss', 'ui-transition', 'context-loss', 'load-loss',
        'suppression-loss', 'target-mode', 'target-disappearance', 'external-guid',
        'world-event', 'key-input', 'passive-cadence', 'mouseover-polling')]
    cases += [dict(name='miss-stays-fresh', policy='miss-stays-fresh', type=0),
              dict(name='cursor-and-focus', policy='cursor-and-focus', type=2)]
    cases += [dict(name=f'presentation-{queries}-reads-{"baseline" if baseline else "trial"}',
                  policy='presentation-amplification', type=2, queries_per_frame=queries,
                  baseline=baseline) for queries in (1, 4) for baseline in (True, False)]
    cases += [dict(name=f'unit-callback-{"baseline" if baseline else "trial"}',
                  policy='unit-publication-callback', type=2, object_kind=9,
                  presentation_callback=True, baseline=baseline) for baseline in (True, False)]
    cases += [dict(name='unit-callback-nested-interact', policy='unit-publication-callback',
                   type=2, object_kind=9, presentation_callback=True, publication_action=True)]
    cases += [dict(name='unit-callback-nested-macro', policy='unit-publication-callback',
                   type=2, object_kind=9, presentation_callback=True, publication_macro=True)]
    report = dict(status='offline-native-hover-continuation', executableSha256=digest,
        fixtureSha256=hashlib.sha256(args.fixture.read_bytes()).hexdigest(),
        nativeActionRegistrations=registrations,nativeActionCalls=native_calls,
        dependencies={name: importlib.metadata.version(name) for name in ('pefile', 'unicorn')},
        cases=[exercise(args.client, args.fixture, case) for case in cases])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'hover continuation: {len(cases)} cases passed; {args.output}')


if __name__ == '__main__': main()
