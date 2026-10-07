#!/usr/bin/env python3
"""Pure tests for tools/sudekimod.py and the descriptor parser of
tools/make_clean_font_mod.py. Synthetic data only (no game files):
    python3 tests/sudekimod_test.py
"""
import io
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import sudekimod  # noqa: E402


def zipcrypto_encrypt(plain, password, check_byte):
    crypt = sudekimod.ZipCrypto(password)
    out = bytearray()
    for byte in bytes(range(11)) + bytes((check_byte,)) + plain:
        temp = (crypt.keys[2] | 2) & 0xffff
        out.append(byte ^ (((temp * (temp ^ 1)) >> 8) & 0xff))
        crypt._update(byte)
    return bytes(out)


def make_tpf(files, comment=b''):
    """Stored, ZipCrypto-encrypted zip with TexMod's XOR layer."""
    body, central = bytearray(), bytearray()
    for name, data in files.items():
        crc = zlib.crc32(data)
        blob = zipcrypto_encrypt(data, sudekimod.TPF_PASSWORD, crc >> 24)
        offset = len(body)
        name_bytes = name.encode()
        body += struct.pack('<IHHHHHIIIHH', 0x04034b50, 20, 1, 0, 0, 0, crc, len(blob), len(data), len(name_bytes), 0)
        body += name_bytes + blob
        central += struct.pack('<IHHHHHHIIIHHHHHII', 0x02014b50, 20, 20, 1, 0, 0, 0, crc, len(blob), len(data),
                               len(name_bytes), 0, 0, 0, 0, 0, offset) + name_bytes
    count = len(files)
    end = struct.pack('<IHHHHIIH', 0x06054b50, 0, 0, count, count, len(central), len(body), len(comment))
    return sudekimod.tpf_unxor(bytes(body + central + end + comment))  # XOR is its own inverse


def png_bytes(width, height, color):
    from PIL import Image
    out = io.BytesIO()
    Image.new('RGBA', (width, height), color).save(out, format='PNG')
    return out.getvalue()


class KeyTests(unittest.TestCase):
    def test_texmod_key_omits_final_inversion(self):
        self.assertEqual(sudekimod.texmod_key(b'123456789'), 0x340BC6D9)

    def test_resource_checksum_is_case_insensitive(self):
        self.assertEqual(sudekimod.resource_checksum('Verdana_16-0.tga'), 0x02AE20DF)
        self.assertEqual(sudekimod.resource_checksum('VERDANA_16-0.TGA'), 0x02AE20DF)

    def test_tga_bottom_up_is_flipped_to_upload_order(self):
        rows = [bytes((1, 2, 3, 4)) * 2, bytes((5, 6, 7, 8)) * 2]  # file order: bottom row first
        header = bytearray(18)
        header[2], header[16], header[17] = 2, 32, 0x08
        header[12:16] = struct.pack('<HH', 2, 2)
        width, height, pixels = sudekimod.tga_level0(bytes(header) + rows[0] + rows[1])
        self.assertEqual((width, height, pixels), (2, 2, rows[1] + rows[0]))
        header[17] = 0x28  # top-down file stays as is
        self.assertEqual(sudekimod.tga_level0(bytes(header) + rows[0] + rows[1])[2], rows[0] + rows[1])
        header[16] = 24
        self.assertIsNone(sudekimod.tga_level0(bytes(header) + rows[0] + rows[1]))

    def test_sqx_level0_is_the_first_mip(self):
        payload = bytes(range(256)) * 2  # 32x32 DXT1 level 0 = 512 bytes
        word = 0x23 | (12 << 8) | (1 << 16) | (5 << 20) | (5 << 24)
        blob = payload + bytes(2048 - len(payload) - 4) + struct.pack('<I', word)
        self.assertEqual(sudekimod.sqx_level0(blob), (32, 32, 'DXT1', payload))
        self.assertIsNone(sudekimod.sqx_level0(blob[:-4] + struct.pack('<I', word & ~0xff)))


class TpfTests(unittest.TestCase):
    def test_round_trip_convert_and_validate(self):
        png = png_bytes(8, 8, (255, 0, 0, 255))
        tpf = make_tpf({'texmod.def': b'0xAC57BC5D|SUDEKI.EXE_0xAC57BC5D.png\r\n0x00000001|missing.dds\r\n',
                        'SUDEKI.EXE_0xAC57BC5D.png': png}, b'Some Author\r\nA description.')
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'Test.tpf'
            source.write_bytes(tpf)
            members, definition, comment = sudekimod.read_tpf(source)
            self.assertEqual(members['SUDEKI.EXE_0xAC57BC5D.png'], png)
            self.assertEqual(sudekimod.parse_texmod_def(definition), [(0xAC57BC5D, 'SUDEKI.EXE_0xAC57BC5D.png'), (1, 'missing.dds')])
            self.assertEqual(comment, 'Some Author\r\nA description.')
            out = Path(tmp) / 'mod'
            self.assertEqual(sudekimod.main(['convert-tpf', str(source), str(out)]), 0)
            sections = sudekimod.read_manifest(out)
            self.assertIn((13, '0xAC57BC5D=textures/0xAC57BC5D.png'), sections['textures'])
            meta = dict(line.split('=', 1) for _, line in sections['mod'])
            self.assertEqual((meta['Format'], meta['Author'], meta['Description']),
                             (sudekimod.FORMAT, 'Some Author', 'A description.'))
            self.assertEqual((out / 'textures/0xAC57BC5D.png').read_bytes(), png)
            self.assertEqual(sudekimod.main(['validate', str(out)]), 0)
            dds_out = Path(tmp) / 'mod-dds'
            self.assertEqual(sudekimod.main(['convert-tpf', str(source), str(dds_out), '--dds']), 0)
            self.assertEqual(sudekimod.image_size((dds_out / 'textures/0xAC57BC5D.dds').read_bytes()[:128]), (8, 8, 'dds'))

    def test_local_header_fallback(self):
        tpf = sudekimod.tpf_unxor(make_tpf({'texmod.def': b'0x2|a.png\n', 'a.png': b'pixels'}))
        damaged = tpf[:tpf.index(b'PK\x01\x02')]  # central directory gone
        entries = sudekimod.scan_local_entries(damaged, sudekimod.TPF_PASSWORD)
        self.assertEqual(entries, {'texmod.def': b'0x2|a.png\n', 'a.png': b'pixels'})


class ManifestTests(unittest.TestCase):
    def test_validate_rejects_unsafe_and_malformed_lines(self):
        with tempfile.TemporaryDirectory() as tmp:
            mod = Path(tmp)
            (mod / 'mod.ini').write_text('[Mod]\nFormat=SudekiMP.Mod/1\n[Textures]\n0x1=../escape.png\n'
                                         'Verdana_16-0.tga=x.png\n0x2=textures/none.png\n')
            self.assertEqual(sudekimod.main(['validate', str(mod)]), 1)

    def test_validate_rejects_unknown_format(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / 'mod.ini').write_text('[Mod]\nFormat=SudekiMP.Mod/2\n[Textures]\n')
            self.assertEqual(sudekimod.main(['validate', tmp]), 1)

    def test_non_ascii_manifest_is_utf16(self):
        with tempfile.TemporaryDirectory() as tmp:
            sudekimod.write_manifest(tmp, {'Name': 'Größe'}, [(5, 'textures/a.png', '')])
            raw = (Path(tmp) / 'mod.ini').read_bytes()
            self.assertEqual(raw[:2], b'\xff\xfe')
            self.assertIn((4, 'Name=Größe'), sudekimod.read_manifest(tmp)['mod'])

    def test_dds_writer_mip_chain(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'a.dds'
            sudekimod.write_dds_argb(Image.new('RGBA', (8, 4), (10, 20, 30, 40)), path)
            data = path.read_bytes()
            self.assertEqual(struct.unpack_from('<I', data, 28)[0], 4)  # 8x4, 4x2, 2x1, 1x1
            self.assertEqual(len(data), 128 + 4 * (32 + 8 + 2 + 1))
            self.assertEqual(data[128:132], bytes((30, 20, 10, 40)))  # BGRA
            self.assertEqual(Image.open(path).convert('RGBA').getpixel((0, 0)), (10, 20, 30, 40))


class DescriptorTests(unittest.TestCase):
    def test_parse_descriptor_layout(self):
        import make_clean_font_mod as font
        header = bytearray(0x54)
        header[:8] = font.DESCRIPTOR_MAGIC
        struct.pack_into('<h', header, 8, -21)
        header[0x1a:0x21] = b'Verdana'
        struct.pack_into('<HHH', header, 0x4a, 0x41, 1, 1)
        blob = bytes(header) + struct.pack('<HHh', 0x41, 0x56, -1) + struct.pack('<HHHHBBbbBb', 0x41, 0, 98, 34, 21, 28, 0, -2, 23, -3)
        parsed = font.parse_descriptor(blob)
        self.assertEqual((parsed['face'], parsed['height']), ('Verdana', -21))
        self.assertEqual(parsed['glyphs'], [{'char': 0x41, 'page': 0, 'x': 98, 'y': 34, 'w': 21, 'h': 28, 'a': -2, 'c': -3}])
        self.assertIsNone(font.parse_descriptor(blob + b'\0'))


class FilesTests(unittest.TestCase):
    def test_add_file_and_validate(self):
        with tempfile.TemporaryDirectory() as tmp:
            mod, source = Path(tmp) / 'mod', Path(tmp) / 'src.bin'
            source.write_bytes(b'model bytes')
            self.assertEqual(sudekimod.main(['add-file', str(mod), 'TAL.HOM', str(source)]), 0)
            self.assertEqual(sudekimod.main(['add-file', str(mod), 'TAL.HOM', str(source)]), 0)  # replaces its line
            self.assertEqual(sudekimod.main(['add-file', str(mod), 'NEW_THING.SQX', str(source)]), 0)
            self.assertEqual(sudekimod.main(['add-file', str(mod), 'sound/Speech/speech_ailish.xwb', str(source)]), 0)
            files = [line for _, line in sudekimod.read_manifest(mod)['files']]
            self.assertEqual(files, ['TAL.HOM=files/TAL.HOM', 'NEW_THING.SQX=files/NEW_THING.SQX',
                                     'sound/Speech/speech_ailish.xwb=files/sound/Speech/speech_ailish.xwb'])
            self.assertTrue((mod / 'files/sound/Speech/speech_ailish.xwb').is_file())
            with self.assertRaises(SystemExit):
                sudekimod.main(['add-file', str(mod), 'data/x.gex', str(source)])
            self.assertEqual((mod / 'files/TAL.HOM').read_bytes(), b'model bytes')
            self.assertEqual(sudekimod.main(['validate', str(mod)]), 0)
            with open(mod / 'mod.ini', 'a') as out:
                out.write('BAD NAME=files/x\r\nX.Y=../up\r\n')
            self.assertEqual(sudekimod.main(['validate', str(mod)]), 1)


if __name__ == '__main__':
    unittest.main()
