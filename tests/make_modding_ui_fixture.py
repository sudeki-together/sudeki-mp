#!/usr/bin/env python3
"""Create a synthetic install for LauncherModsUiTest; no retail data involved."""
import io
import struct
import sys
from pathlib import Path
from PIL import Image
from modding_roundtrip_test import archive, tga

root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
face = 'CL002_Tal_Test_FaceLR.TGA'
model = 'Tal_Test.HOM'
payload = archive([(face, tga(True)), ('Nameless.TGA', tga(False)), (model, b'synthetic HOM data')])
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
