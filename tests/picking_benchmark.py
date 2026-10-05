"""Export read-only real M2/SKIN geometry and the actual 12340 triangle predicate.

No client process is launched. The x86 fixture relocates only the predicate's
absolute epsilon load; all instructions and relative branches stay unchanged.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def export(client, pairs, output):
    output.mkdir(parents=True, exist_ok=True)
    exe = client.read_bytes()
    pe = struct.unpack_from('<I', exe, 0x3c)[0]
    sections, optional = struct.unpack_from('<HH', exe, pe + 6)[0], struct.unpack_from('<H', exe, pe + 20)[0]
    image_base = struct.unpack_from('<I', exe, pe + 24 + 28)[0]
    def read_va(va, count):
        rva = va - image_base
        for i in range(sections):
            virtual_size, address, raw_size, raw = struct.unpack_from('<4I', exe, pe + 24 + optional + i * 40 + 8)
            if address <= rva and rva + count <= address + raw_size:
                return exe[raw + rva - address:raw + rva - address + count]
        raise ValueError(hex(va))
    code = read_va(0x81d510, 0x168)
    if code.count(struct.pack('<I', 0x9ea558)) != 1 or code[-3:] != b'\xc2\x20\x00':
        raise ValueError('Unexpected 12340 triangle predicate')
    (output / 'native.bin').write_bytes(code + read_va(0x9ea558, 4))
    (output/'fillers.bin').write_bytes(read_va(0x81d2c0, 0x830))
    (output/'transform.bin').write_bytes(read_va(0x4c21b0, 0x5d))
    (output/'blend-scale.bin').write_bytes(read_va(0xa45564, 4))
    source = Path(__file__).parents[1]/'src/client/CM2Shared/WideIndices.cpp'
    text = source.read_text()
    begin = text.index('    bool RefillPickingPositions(')
    end = text.index('\n    void PreparePickingSection(', begin)
    (output/'refill.generated.hpp').write_text(text[begin:end]+'\n')
    def declaration(marker):
        first = text.index(marker)
        brace = text.index('{', first)
        depth, last = 1, brace+1
        while depth:
            depth += (text[last] == '{') - (text[last] == '}')
            last += 1
        if text[last:last+1] == ';': last += 1
        return text[first:last]+'\n'
    (output/'types.generated.hpp').write_text(declaration('    struct PickingSource\n')+
        declaration('    enum class PickingReject')+declaration('    struct PickingCall\n'))
    (output/'prepare.generated.hpp').write_text(declaration('    bool WideVertexStart(')+
        declaration('    window::SectionPlacement Placement(')+declaration('    uint32_t TriangleStart(')+
        declaration('    void PreparePickingSection(')+declaration('    void PrepareLegacySection('))
    (output/'fill-hook.generated.hpp').write_text(declaration('    template <unsigned Filler>\n'))
    manifest = dict(client=str(client), clientSha256=hashlib.sha256(exe).hexdigest(),
                    predicateAddress='0x81d510', predicateSha256=hashlib.sha256(code).hexdigest(),
                    refillSource=str(source), refillSha256=hashlib.sha256(text[begin:end].encode()).hexdigest(),
                    models=[])
    manifest['nativeCaptures'] = {name: hashlib.sha256((output/name).read_bytes()).hexdigest()
        for name in ('native.bin', 'fillers.bin', 'transform.bin', 'blend-scale.bin')}
    manifest['generatedProduction'] = {name: hashlib.sha256((output/name).read_bytes()).hexdigest()
        for name in ('types.generated.hpp', 'refill.generated.hpp', 'prepare.generated.hpp', 'fill-hook.generated.hpp')}
    for name, model, skin in pairs:
        m, s = model.read_bytes(), skin.read_bytes()
        vcount, voff = struct.unpack_from('<II', m, 0x3c)
        (output/(name+'.vertices')).write_bytes(m[voff:voff+vcount*48])
        lookup_count, lookup_offset, index_count, index_offset = struct.unpack_from('<4I', s, 4)
        section_count, section_offset, batch_count, batch_offset = struct.unpack_from('<4I', s, 28)
        lookup = struct.unpack_from('<' + str(lookup_count) + 'H', s, lookup_offset)
        (output/(name+'.lookup')).write_bytes(s[lookup_offset:lookup_offset+lookup_count*2])
        indices = struct.unpack_from('<' + str(index_count) + 'H', s, index_offset)
        batches = [struct.unpack_from('<BB11H', s, batch_offset+i*24) for i in range(batch_count)]
        # Native filtering keeps layer zero and excludes the no-pick bit. Hidden sections are
        # supplied separately to the fixture, as masks; a file cannot establish live visibility.
        eligible = {b[3] for b in batches if b[7] == 0 and not b[0] & 8}
        data = bytearray(struct.pack('<I', len(eligible)))
        first = 0
        for i in range(section_count):
            sid, level, low, count, start, n = struct.unpack_from('<6H', s, section_offset+i*48)
            wide = (level << 16) | start
            if wide > index_count or n > index_count-wide: wide = start
            if i in eligible:
                raw = indices[wide:wide+n]
                local = [(x-low)&65535 for x in raw]
                if any(x >= count for x in local): raise ValueError((name, i, 'invalid window'))
                influences = struct.unpack_from('<H', s, section_offset+i*48+16)[0]
                data += struct.pack('<5I', sid, first, count, n, influences)
                for k in range(count):
                    # Dense source for >65K, native lookup for ordinary/index-only skins.
                    v = first+k if lookup_count > 65536 else lookup[low+k]
                    if v >= vcount: raise ValueError((name, i, v))
                    data += m[voff+v*48:voff+v*48+12]
                data += struct.pack('<' + str(n) + 'H', *local)
            first += count
        path = output / (name+'.bin')
        path.write_bytes(data)
        manifest['models'].append(dict(name=name, model=str(model), skin=str(skin), vertices=vcount,
            skinVertices=lookup_count, triangles=index_count//3, sections=section_count,
            eligibleSections=len(eligible), modelSha256=hashlib.sha256(m).hexdigest(),
            skinSha256=hashlib.sha256(s).hexdigest()))
    (output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    return manifest


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--client', type=Path, required=True)
    p.add_argument('--assets', type=Path, required=True, help='Existing authoritative wow/_ssot')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--installed-client', type=Path, help='Read target archives via existing Velora StormLib owner')
    a = p.parse_args()
    pairs = []
    if a.installed_client:
        repo = a.assets.parents[1]
        sys.path.insert(0, str(repo))
        from wow.shared.formats.mpq import StormLib, DEFAULT_STORMLIB
        storm = StormLib(DEFAULT_STORMLIB)
        extracted = a.output/'installed-readback'
        extracted.mkdir(parents=True, exist_ok=True)
        receipts = []
        for name in ['tifa', 'elene']:
            target = json.loads((repo/'wow/shared/assets/native-targets'/f'{name}.json').read_text())['native']
            archive = a.installed_client/'Data'/target['patchSlot']
            stem = target['gameRoot']+'\\'+target['modelName']
            model, skin = extracted/(name+'.M2'), extracted/(name+'00.skin')
            with storm.archive(archive, readonly=True) as handle:
                model.write_bytes(storm.read(handle, stem+'.M2'))
                skin.write_bytes(storm.read(handle, stem+'00.skin'))
            receipts.append(dict(name=name, archive=str(archive), modelMember=stem+'.M2', skinMember=stem+'00.skin'))
            pairs.append(('installed-'+name, model, skin))
        (a.output/'installed-readback.json').write_text(json.dumps(receipts, indent=2)+'\n')
    for kind, name in [('receivers', 'tifa'), ('receivers', 'kasumi'), ('receivers', 'ayane'),
                       ('receivers', 'shadowheart'), ('receivers', 'luna'), ('bodies', 'human-female')]:
        folder = a.assets/kind/name/'model'
        pairs.append((name, next(folder.glob('*.M2')), next(folder.glob('*.skin'))))
    print(json.dumps(export(a.client, pairs, a.output), indent=2))
