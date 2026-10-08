#!/usr/bin/env python3
"""Create a synthetic install for LauncherModsUiTest; no retail data involved."""
import io
import struct
import sys
from pathlib import Path
from PIL import Image
from modding_roundtrip_test import archive, tga
from sudekimod_test import make_tpf
import zipfile

root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
face = 'CL002_Tal_Test_FaceLR.TGA'
model = 'Tal_Test.HOM'


def hom(texture_names):
    """HOM v5 with one texture-name chunk (kind 24), as the game stores it."""
    strings = b''.join(n.encode('ascii') + b'\0' for n in texture_names)
    offsets, cursor = [], 0
    for n in texture_names:
        offsets.append(cursor)
        cursor += len(n) + 1
    chunk = struct.pack('<2I', len(texture_names), len(strings)) + struct.pack(f'<{len(offsets)}I', *offsets) + strings
    header = b'HOM\x05' + struct.pack('<3I', 1, 0, 0) + struct.pack('<I', (24 << 24) | len(chunk))
    return header.ljust(2048, b'\0') + chunk.ljust(2048, b'\0')


# The model names a spec map that does not exist, then the face (variant !1):
# the launcher previews the model with the face texture.
payload = archive([(face, tga(True)), ('Nameless.TGA', tga(False)),
                   (model, hom(['Tal_Test_spec', face[:-4] + '!1']))])
(root / 'Synthetic.baf').write_bytes(payload)
(root / 'original-archive.bin').write_bytes(payload)
(root / 'SUDEKI.exe').write_bytes(f'{face}\0{model}\0'.encode())
(root / 'SudekiMP.ini').write_text('[Mods]\nEnable=true\nFolder=mods\n', encoding='ascii')
for name in ('A Base', 'Z Override'):
    folder = root / 'mods' / name
    folder.mkdir(parents=True)
    (folder / 'mod.ini').write_text(f'[Mod]\nFormat=SudekiMP.Mod/1\nName={name}\nAuthor=Preserve Me\nEnabled=true\n[Textures]\n', encoding='ascii')
image = Image.new('RGBA', (8, 4), (240, 50, 10, 255))
image.save(root / 'replacement.png')
image.convert('RGB').save(root / 'replacement.jpg')
image.save(root / 'replacement.bmp')
(root / 'bad.png').write_bytes((root / 'replacement.png').read_bytes()[:-5])
# Import fixtures: a TexMod package, a zipped mod folder, and a zip that
# tries to write outside its folder.
png = (root / 'replacement.png').read_bytes()
(root / 'package.tpf').write_bytes(make_tpf({'texmod.def': b'0x12345678|SUDEKI.EXE_0x12345678.png\r\n',
                                             'SUDEKI.EXE_0x12345678.png': png}, b'Tester\r\nSynthetic package'))
with zipfile.ZipFile(root / 'pack.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('Packed/mod.ini', '[Mod]\r\nFormat=SudekiMP.Mod/1\r\nName=Packed\r\n[Textures]\r\n0x12345678=textures/x.png\r\n')
    z.writestr('Packed/textures/x.png', png)
with zipfile.ZipFile(root / 'evil.zip', 'w') as z:
    z.writestr('Evil/mod.ini', '[Mod]\r\nFormat=SudekiMP.Mod/1\r\n')
    z.writestr('Evil/../escape.txt', 'outside')
