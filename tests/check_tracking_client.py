"""Read-only compatibility report for supplied client files; never edits or starts them."""
import hashlib
import json
import re
from pathlib import Path
import pefile

HEADER = Path(__file__).resolve().parents[1] / 'src/offsets/game/MinimapTracking.hpp'


def verify(path):
    path = Path(path)
    pe = pefile.PE(str(path))
    if pe.OPTIONAL_HEADER.ImageBase != 0x400000 or pe.FILE_HEADER.Machine != 0x14c:
        raise ValueError('Expected fixed-base Win32 client')
    sites = re.findall(r'\{"(\w+)", (0x[\dA-F]+), (0x[\dA-F]+), (0x[\dA-F]+)ULL\}', HEADER.read_text())
    if not sites:
        raise ValueError('No compatibility fingerprints found')
    results = []
    for name, address, size, expected in sites:
        address, size, expected = (int(v, 16) for v in (address, size, expected))
        data = pe.get_data(address - 0x400000, size)
        value = 0xcbf29ce484222325
        for byte in data:
            value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
        if len(data) != size or value != expected:
            raise ValueError('Tracking compatibility failed: ' + name)
        results.append(dict(name=name, address=hex(address), size=size, sha256=hashlib.sha256(data).hexdigest()))
    return dict(client=str(path.resolve()), sha256=hashlib.sha256(path.read_bytes()).hexdigest(), sites=results)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('client', type=Path, nargs='+')
    print(json.dumps([verify(p) for p in parser.parse_args().client], indent=2))
