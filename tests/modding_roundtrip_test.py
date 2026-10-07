#!/usr/bin/env python3
"""Compare C and Python catalog + manifest on synthetic data only.
Usage: python3 tests/modding_roundtrip_test.py /path/to/host/ModdingRoundtrip
"""
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import sudekimod


def archive(resources):
    payload = bytearray()
    buckets = [[] for _ in range(256)]
    for name, data in resources:
        key = sudekimod.resource_checksum(name)
        buckets[key & 255].append((len(payload), len(data), key))
        payload += data
    table = bytearray()
    for rows in buckets:
        rows.sort(key=lambda row: row[2])
        table += struct.pack('<I', len(rows))
        for row in rows:
            table += struct.pack('<III', *row)
    footer = struct.pack('<III', 0x04666162, len(table), len(resources)) + bytes(2048)
    return payload + table + footer


def tga(top_down):
    header = bytearray(18)
    header[2], header[16], header[17] = 2, 32, 0x28 if top_down else 8
    struct.pack_into('<HH', header, 12, 2, 2)
    return bytes(header) + bytes(range(16))


def main():
    helper = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        resources = [('Hero_Face.SQX', bytes(range(32)) + bytes(2012) +
                      struct.pack('<I', 0x23 | (15 << 8) | (3 << 20) | (2 << 24))),
                     ('Bottom.tga', tga(False)), ('Top.tga', tga(True)),
                     ('SyntheticFont_16-0.tga', tga(True)), ('Mystery.SQX', bytes(range(8)) + bytes(2036) +
                      struct.pack('<I', 0x23 | (12 << 8) | (2 << 20) | (2 << 24)))]
        (root / 'Test.baf').write_bytes(archive(resources))
        (root / 'SUDEKI.exe').write_bytes(b'Hero_Face!1\0Bottom.tga\0Top.tga\0SyntheticFont_16\0')
        python_rows = sorted((f"0x{e['key']:08X}", e['name'], f"0x{e['archive_key']:08X}",
                              str(e['width']), str(e['height']), e['format'])
                             for e in sudekimod.build_catalog(root, log=lambda _: None))
        result = subprocess.check_output([helper, 'catalog', str(root / 'Test.baf'),
                                          str(root / 'SUDEKI.exe')], text=True)
        c_rows = sorted(tuple(line.split('\t')) for line in result.splitlines())
        assert c_rows == python_rows, (c_rows, python_rows)
        mod = root / 'mod'
        (mod / 'textures').mkdir(parents=True)
        (mod / 'files').mkdir()
        (mod / 'textures' / 'old.tga').write_bytes(tga(True))
        (mod / 'textures' / 'new.tga').write_bytes(tga(False))
        (mod / 'files' / 'new.hom').write_bytes(b'synthetic model')
        sudekimod.write_manifest(mod, {'Name': 'Größe 🐾'}, [(0x12, 'textures/old.tga', 'preserved comment')])
        original = mod / 'before.ini'
        original.write_bytes((mod / 'mod.ini').read_bytes())
        subprocess.check_call([helper, 'manifest', str(original), str(mod / 'mod.ini')])
        sections = sudekimod.read_manifest(mod)
        assert (mod / 'mod.ini').read_bytes().startswith(b'\xff\xfe')
        meta = dict(line.split('=', 1) for _, line in sections['mod'])
        assert meta['Name'] == 'Größe 🐾' and meta['Enabled'] == 'false'
        assert [line for _, line in sections['textures']] == ['0x00000012=textures/new.tga']
        assert [line for _, line in sections['files']] == ['BAR.HOM=files/new.hom']
        assert sudekimod.main(['validate', str(mod)]) == 0
    print('C/Python round trip: catalog + UTF-16 manifest passed')


if __name__ == '__main__':
    main()
