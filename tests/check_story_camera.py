"""Compiled camera controller over two resident fixtures; no rendered acceptance."""
import argparse
import json
from pathlib import Path
import struct
from check_story_residents import Fixture


def setup(client, library):
    f = Fixture(client, library); f.roster()
    assert f.status('BeginPair', 1) == 'loading'
    token = f.call('PairToken'); assert f.step(token) == 'ready'
    shared = f.get(f.BACKGROUND + 0x2C); header = f.get(shared + 0x150)
    f.put(f.BACKGROUND + 0x10, 1)
    f.cpu.mem_write(shared + 0x3C, b'Interface\\Glues\\Models\\UI_Human\\UI_Human\0')
    f.put(f.get(header + 0x20) + 4, 1067); f.put(header + 0x110, 2)
    table, original, trial, stem = (f.MEMORY + offset for offset in (0x92000, 0x93000, 0x94000, 0x95000))
    f.put(f.BACKGROUND + 0x2B4, table)
    f.put(table + 0x34, original); f.put(table + 56 + 0x34, trial)
    f.put(original + 4, 2); f.put(trial + 4, 1); f.put(f.FRAME + 0x2A4, original)
    f.cpu.mem_write(stem, b'Interface\\Glues\\Models\\UI_Human\\UI_Human\0')
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
    return dict(passed=cases, limits='Compiled owning controller and native camera helpers; synthetic appearance/readiness/sequence leaves. No rendering or attachment-motion verdict.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args(); report = exercise(args.client, args.fixture)
    text = json.dumps(report, indent=2) + '\n'
    if args.output: args.output.write_text(text, encoding='utf-8')
    print(text)
