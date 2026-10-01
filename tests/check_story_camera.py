"""Compiled camera controller over two resident fixtures; no rendered acceptance."""
import argparse
import json
from pathlib import Path
import struct
from check_story_residents import Fixture
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP

MODEL_PATH = b'Interface\\Glues\\Models\\UI_Human\\UI_Human.m2'


def native_model_path(f, path):
    """Original cache path normalization/copy; file/allocator/load leaves only."""
    cache, input_path, shared = (f.MEMORY + offset for offset in (0x9a000, 0x9d000, 0x9f000))
    f.cpu.mem_write(input_path, path + b'\0')
    leaves = dict(f.leaves)
    f.leaves[0x4bb3e0] = lambda: f.ret(1)  # model bounds query
    def opened(): f.put(f.arg(4), 1); f.ret(1, 16)
    f.leaves[0x424b50] = opened
    f.leaves[0x76e540] = lambda: f.ret(shared, 16)
    f.leaves[0x83c5f0] = lambda: f.ret(shared, 4)
    f.leaves[0x83d410] = lambda: f.ret(1, 12)
    try:
        sp = f.STACK + 0xf000
        for offset, value in ((0, f.STOP), (4, input_path), (8, 0)): f.put(sp + offset, value)
        f.cpu.reg_write(UC_X86_REG_ESP, sp); f.cpu.reg_write(UC_X86_REG_ECX, cache)
        f.cpu.emu_start(0x81c390, 0x81c64c, count=100000)
        assert f.cpu.reg_read(UC_X86_REG_EIP) == 0x81c64c
        return bytes(f.cpu.mem_read(shared + 0x3c, 276)).split(b'\0', 1)[0]
    finally:
        f.leaves.clear(); f.leaves.update(leaves)


def setup(client, library, count=2):
    f = Fixture(client, library); f.roster(count)
    if count==2: assert f.status('BeginPair', 1) == 'loading'
    else:
        requests=f.MEMORY+0x91000
        for index in range(count):
            f.cpu.mem_write(requests+index*40,struct.pack('<I4xQffIff4x',index,0x1111+index,index*.4,0,0,-1,-1))
        assert f.status('BeginPage',requests,count,0)=='loading'
    token = f.call('PairToken'); assert f.step(token) == 'ready'
    shared = f.get(f.BACKGROUND + 0x2C); header = f.get(shared + 0x150)
    f.put(f.BACKGROUND + 0x10, 1)
    loaded_path = native_model_path(f, MODEL_PATH)
    assert loaded_path == MODEL_PATH.lower()  # .m2 is retained, not a stem.
    f.cpu.mem_write(shared + 0x3C, loaded_path + b'\0')
    f.put(header + 0x1C, 1)
    f.cpu.mem_write(f.get(header + 0x20), struct.pack('<HHIfI', 0, 0, 1067, 0, 0x20))
    f.put(header + 0x110, 2)
    table, original, trial, stem = (f.MEMORY + offset for offset in (0x92000, 0x93000, 0x94000, 0x95000))
    f.put(f.BACKGROUND + 0x2B4, table)
    f.put(table + 0x34, original); f.put(table + 56 + 0x34, trial)
    f.put(original + 4, 2); f.put(trial + 4, 1); f.put(f.FRAME + 0x2A4, original)
    f.cpu.mem_write(stem, MODEL_PATH + b'\0')
    f.leaves[0x824F00] = lambda: f.ret(1, 8)
    f.camera_events = []
    def sequence():
        f.camera_events.append((f.this(), f.arg(2), f.arg(4)))
        f.sequence()
    f.leaves[0x832AB0] = sequence
    return f, token, original, trial, stem


def exercise(client, library):
    cases = []
    f, token, original, trial, stem = setup(client, library)
    f.cpu.mem_write(stem, MODEL_PATH.rsplit(b'.', 1)[0] + b'\0')
    assert f.status('CameraPair', token, stem, 1000) == 'camera-unavailable'
    assert f.call('PairCameraActive') == 0 and not f.camera_events
    assert f.call('PairToken') == token and f.get(f.FRAME + 0x2A4) == original
    f.cpu.mem_write(stem, MODEL_PATH + b'\0')
    assert f.status('CameraPair', token, stem, 1000) == 'camera'
    f.call('StopCameraPair')
    cases.append('native-cache-keeps-m2-extension-stem-only-refuses-full-path-starts')
    f, token, original, trial, stem = setup(client, library)
    actor = f.selected_model
    assert f.status('CameraPair', token, stem, 1000) == 'camera'
    assert f.get(f.FRAME + 0x2A4) == trial and f.call('PairCameraActive') == 1
    assert f.status('ActPair', token, 0, 1) == 'salute'
    for _ in range(20): assert f.step(token, .05) == 'ready'
    assert f.call('PairCameraActive') == 0 and f.get(f.FRAME + 0x2A4) == original
    assert f.get(original + 4) == 2 and f.get(trial + 4) == 1
    times = [time for model, clip, time in f.camera_events if model == f.BACKGROUND]
    assert times[0] == times[-1] == 0 and any(0 < time < 1000 for time in times)
    assert f.get(0xAC436C) == 0 and any(model == actor and clip == 113 for model, clip, _ in f.camera_events)
    cases.append('bounded-camera-path-restores-native-camera-and-time-with-independent-resident-salute')

    for operation in ('manual', 'stop', 'select', 'refresh', 'initialize'):
        f, token, original, trial, stem = setup(client, library)
        assert f.status('CameraPair', token, stem, 1000) == 'camera'
        if operation == 'manual': f.call('StopCameraPair')
        elif operation == 'stop': f.call('StopPair')
        elif operation == 'select': assert f.call('SelectPair', 1) == 77
        elif operation == 'refresh': f.call('RefreshPair')
        else: f.call('InitializeNormal')
        assert f.call('PairCameraActive') == 0 and f.get(f.FRAME + 0x2A4) == original
        assert (f.BACKGROUND, 0, 0) in f.camera_events
        assert f.get(trial + 4) == 1
    cases.append('manual-stop-pair-stop-selection-refresh-native-initializer-cancel-before-forwarding')

    for replacement in ('background', 'frame', 'camera', 'rows'):
        f, token, original, trial, stem = setup(client, library)
        assert f.status('CameraPair', token, stem, 1000) == 'camera'
        f.camera_events.clear()
        if replacement == 'background': f.put(f.FRAME + 0x2A0, f.MEMORY + 0x96000)
        elif replacement == 'frame': f.put(0xB6B1FC, f.MEMORY + 0x97000)
        elif replacement == 'camera': f.put(f.FRAME + 0x2A4, f.MEMORY + 0x98000)
        else: f.put(0xB6B240, f.MEMORY + 0x99000)
        f.call('StopPair')
        assert f.call('PairCameraActive') == 0
        if replacement in ('frame', 'background'):
            assert not any(model == f.BACKGROUND for model, _, _ in f.camera_events)
        elif replacement == 'camera':
            assert f.get(f.FRAME + 0x2A4) == f.MEMORY + 0x98000
            assert (f.BACKGROUND, 0, 0) in f.camera_events
        else: assert f.get(f.FRAME + 0x2A4) == original
    cases.append('replaced-frame-background-camera-and-roster-do-not-write-stale-camera-ownership')

    for refusal in ('token', 'duration', 'source', 'stock-camera'):
        f, token, original, trial, stem = setup(client, library)
        if refusal == 'source': f.cpu.mem_write(stem, b'WrongScene\0')
        if refusal == 'stock-camera': f.put(f.FRAME + 0x2A4, 0)
        status = f.status('CameraPair', token + (refusal == 'token'), stem, 1067 if refusal == 'duration' else 1000)
        assert status == ('stale-generation' if refusal == 'token' else 'camera-unavailable')
        assert f.call('PairCameraActive') == 0 and not f.camera_events
        assert f.get(0xAC436C) == 0 and f.call('PairToken') == token
    cases.append('stale-token-source-duration-and-camera-refusals-preserve-residents-and-selection')
    f, token, original, trial, stem = setup(client, library,10)
    assert f.call('PageCount')==10 and f.status('CameraPair',token,stem,1000)=='camera'
    for _ in range(20): assert f.step(token,.05)=='ready'
    assert f.get(f.FRAME+0x2a4)==original and f.call('PageCount')==10
    assert f.status('CameraPair',token,stem,1000)=='camera'
    assert f.call('SelectPair',9)==77 and f.get(0xAC436C)==9
    assert f.get(f.FRAME+0x2a4)==original and f.call('PageCount')==0
    for index in range(1,10):
        actor=f.get(f.get(f.ROWS+index*0x198+0x188)+0x38)
        assert f.get(actor+0x48)==0 and f.get(actor+0x2ac)==0x4e3a20
    cases.append('ten-resident-camera-expiry-and-active-selection-restore-entire-group-before-native-target')
    f, token, original, trial, stem = setup(client,library,5)
    assert f.status('HoldCameraPair',token,stem,800)=='camera'
    assert f.camera_events[-1]==(f.BACKGROUND,0,800) # no replayed close-to-wide pan on selection
    for _ in range(240): assert f.step(token,.05)=='ready'
    assert f.call('PairCameraActive')==1 and f.get(f.FRAME+0x2a4)==trial
    assert f.camera_events[-1]==(f.BACKGROUND,0,800)
    f.call('StopPair')
    assert f.call('PairCameraActive')==0 and f.get(f.FRAME+0x2a4)==original
    cases.append('persistent-harbour-camera-clamps-wide-time-and-restores-on-scene-stop')
    return dict(passed=cases, limits='Compiled owning controller and native camera/cache-path instructions; synthetic appearance/readiness/sequence and cache file/allocator/load leaves. No rendering or attachment-motion verdict.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args(); report = exercise(args.client, args.fixture)
    text = json.dumps(report, indent=2) + '\n'
    if args.output: args.output.write_text(text, encoding='utf-8')
    print(text)
