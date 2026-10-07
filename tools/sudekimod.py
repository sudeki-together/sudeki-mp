#!/usr/bin/env python3
"""SudekiMP texture mod packages: convert TexMod TPFs, catalog keys, build, validate.

Format: docs/texture-mods.md. A package is a folder with mod.ini and image
files; the runtime ([TextureMods] in SudekiMP.ini) loads every package under
the game's mods folder. Textures are keyed by the TexMod-compatible CRC-32 of
the level-0 pixels the game uploads, so TexMod keys carry over unchanged.

Commands:
  convert-tpf MOD.tpf OUT_DIR [--game DIR] [--dds]   TexMod package -> SudekiMP mod
  catalog --game DIR [--out keys.tsv]                 key <-> archive resource name
  build SRC_DIR OUT_DIR --game DIR [--dds]            images named NAME.EXT.<img> -> mod
  validate MOD_DIR                                    check a package

Only Python's standard library is required; --dds needs Pillow. Nothing here
reads or writes game archives except the read-only catalog scan, and no game
data is copied anywhere but the output folder you choose.
"""
import argparse
import io
import mmap
import re
import struct
import sys
import zipfile
import zlib
from pathlib import Path

FORMAT = 'SudekiMP.Mod/1'
IMAGE_EXTENSIONS = ('.dds', '.png', '.tga', '.bmp', '.jpg', '.jpeg')
TPF_XOR = bytes((0xa4, 0x3f, 0xa4, 0x3f))  # 0x3FA43FA4 little-endian, whole file
# The ZipCrypto password every TexMod TPF uses (as published by OpenTexMod/uMod).
TPF_PASSWORD = bytes((
    0x73, 0x2A, 0x63, 0x7D, 0x5F, 0x0A, 0xA6, 0xBD, 0x7D, 0x65, 0x7E, 0x67, 0x61, 0x2A,
    0x7F, 0x7F, 0x74, 0x61, 0x67, 0x5B, 0x60, 0x70, 0x45, 0x74, 0x5C, 0x22, 0x74, 0x5D,
    0x6E, 0x6A, 0x73, 0x41, 0x77, 0x6E, 0x46, 0x47, 0x77, 0x49, 0x0C, 0x4B, 0x46, 0x6F))


def texmod_key(data):
    """CRC-32 without the final inversion, over the level-0 bytes."""
    return zlib.crc32(data) ^ 0xffffffff


def resource_checksum(name):
    """Archive key of NAME.EXT (upper-case ASCII, alternate add/multiply)."""
    value = 0
    for index, byte in enumerate(name.upper().encode('ascii')):
        value = (value * byte if index % 2 else value + byte) & 0xffffffff
    return value


# ---------------------------------------------------------------- archives

def index_archive(data):
    """BAF index: 256 buckets of {offset,size,key} before a 2060-byte footer."""
    if len(data) < 2060:
        raise ValueError('truncated BAF')
    footer = len(data) - 2060
    magic, table_size, declared = struct.unpack_from('<III', data, footer)
    if magic != 0x04666162 or not 1024 <= table_size <= footer:
        raise ValueError('unsupported BAF footer')
    table_start = cursor = footer - table_size
    records, seen = [], set()
    for bucket in range(256):
        count = struct.unpack_from('<I', data, cursor)[0]
        cursor += 4
        if cursor + count * 12 > footer:
            raise ValueError('bucket crosses footer')
        for _ in range(count):
            offset, size, key = struct.unpack_from('<III', data, cursor)
            cursor += 12
            if offset + size > table_start or key & 255 != bucket or key in seen:
                raise ValueError('invalid index record')
            seen.add(key)
            records.append((offset, size, key))
    if cursor != footer or len(records) != declared:
        raise ValueError('index extent/count mismatch')
    return records


def tga_level0(data):
    """Uncompressed 32-bit TGA -> (width, height, top-down BGRA bytes) or None.

    The game uploads A8R8G8B8 rows top-down; a bottom-up file (descriptor bit
    5 clear) is flipped. Confirmed by the SymphonyUI TPF key 0xAC57BC5D, which
    is this hash of Fonts.baf Verdana_16-0.tga (a bottom-up file)."""
    if len(data) < 18 or data[1] != 0 or data[2] != 2 or data[16] != 32:
        return None
    width, height = struct.unpack_from('<HH', data, 12)
    start = 18 + data[0]
    size = width * height * 4
    if not width or not height or start + size > len(data):
        return None
    pixels = data[start:start + size]
    if not data[17] & 0x20:
        row = width * 4
        pixels = b''.join(pixels[i * row:(i + 1) * row] for i in range(height - 1, -1, -1))
    return width, height, pixels


SQX_FORMATS = {12: ('DXT1', 8), 14: ('DXT3', 16), 15: ('DXT5', 16)}


def sqx_level0(data):
    """SQX (Xbox pixel container) DXT1/3/5 -> (width, height, format, level-0 blocks)."""
    if len(data) < 2048 or len(data) % 2048:
        return None
    word = struct.unpack_from('<I', data, len(data) - 4)[0]
    fmt = (word >> 8) & 255
    if word & 255 != 0x23 or word >> 28 or fmt not in SQX_FORMATS:
        return None
    width, height = 1 << ((word >> 20) & 15), 1 << ((word >> 24) & 15)
    name, block = SQX_FORMATS[fmt]
    size = max(1, width // 4) * max(1, height // 4) * block
    return (width, height, name, data[:size]) if size <= len(data) - 4 else None


NAME_PATTERN = re.compile(rb'([A-Za-z0-9_\-]{2,120}\.(?:SQX|TGA|sqx|tga))(?![A-Za-z0-9_])')
FONT_BASE = re.compile(rb'(?<![A-Za-z0-9_])([A-Za-z]{3,32}_\d{1,2})\x00')
# Materials name textures without the extension (the loader appends .SQX), and
# zone materials may carry a '!N' variant suffix that resolves to the base.
BARE_NAME = re.compile(rb'(?<![A-Za-z0-9_\-])([A-Za-z][A-Za-z0-9_\-]{2,119})(?:![0-9]+)?(?![A-Za-z0-9_\-.])')


def harvest_names(blobs, keys):
    """Candidate NAME.EXT strings whose checksum is one of the archive keys."""
    names = set()
    for blob in blobs:
        for match in NAME_PATTERN.finditer(blob):
            names.add(match.group(1).decode('ascii'))
        for match in FONT_BASE.finditer(blob):  # font pages: Verdana_16 -> Verdana_16-0.tga
            names.update(f"{match.group(1).decode('ascii')}-{page}.tga" for page in range(4))
        for token in set(BARE_NAME.findall(blob)):
            stem = token.decode('ascii')
            names.update((stem + '.SQX', stem + '.TGA'))
    return {n for n in names if resource_checksum(n) in keys}


def game_archives(game):
    game = Path(game)
    found = sorted(p for p in game.iterdir() if p.suffix.lower() == '.baf' and p.is_file())
    if not found:
        raise SystemExit(f'no .baf archives in {game}')
    return found


def build_catalog(game, log=print):
    """Every TGA/SQX texture in the game's archives with its TexMod key."""
    game = Path(game)
    archives = game_archives(game)
    entries = []
    blobs = []
    exe = next((p for p in game.iterdir() if p.name.lower() == 'sudeki.exe'), None)
    handles = []
    try:
        if exe:
            blobs.append(exe.read_bytes())
        for path in archives:
            stream = path.open('rb')
            data = mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ)
            handles.append((stream, data))
            blobs.append(data)
        indexes = [index_archive(data) for _, data in handles]
        names = harvest_names(blobs, {key for index in indexes for _, _, key in index})
        by_key = {}
        for name in names:
            by_key.setdefault(resource_checksum(name), set()).add(name)
        for path, (_, data), index in zip(archives, handles, indexes):
            counts = [0, 0]
            for offset, size, archive_key in index:
                payload = data[offset:offset + size]
                level = tga_level0(payload)
                if level:
                    width, height, pixels = level
                    fmt = 'A8R8G8B8'
                else:
                    level = sqx_level0(payload)
                    if not level:
                        continue
                    width, height, fmt, pixels = level
                key = texmod_key(pixels)
                # Keys are case-insensitive; keep one spelling per distinct name.
                spellings = {}
                for candidate in sorted(by_key.get(archive_key, ())):
                    spellings.setdefault(candidate.upper(), candidate)
                candidates = list(spellings.values())
                entries.append({'key': key, 'name': candidates[0] if len(candidates) == 1 else '',
                                'archive': path.name, 'archive_key': archive_key,
                                'width': width, 'height': height, 'format': fmt})
                counts[len(candidates) == 1] += 1
            log(f'{path.name}: {sum(counts)} textures, {counts[1]} named')
    finally:
        for stream, data in handles:
            data.close()
            stream.close()
    return entries


def write_catalog(entries, path):
    with (sys.stdout if path == '/dev/stdout' else open(path, 'w', encoding='ascii', newline='\n')) as out:
        out.write('key\tname\tarchive\tarchive_key\twidth\theight\tformat\n')
        for e in sorted(entries, key=lambda e: (e['archive'], e['name'] or '~', e['key'])):
            out.write(f"0x{e['key']:08X}\t{e['name']}\t{e['archive']}\t0x{e['archive_key']:08X}\t"
                      f"{e['width']}\t{e['height']}\t{e['format']}\n")


def catalog_lookup(entries):
    by_key, by_name = {}, {}
    for e in entries:
        by_key.setdefault(e['key'], e)
        if e['name']:
            by_name.setdefault(e['name'].upper(), e)
    return by_key, by_name


# ---------------------------------------------------------------- TexMod TPF

def tpf_unxor(raw):
    data = bytearray(raw)
    words = len(data) // 4 * 4
    for i in range(4):
        data[i:words:4] = bytes(b ^ TPF_XOR[i] for b in data[i:words:4])
    for i in range(words, len(data)):
        data[i] ^= TPF_XOR[0]
    return bytes(data)


class ZipCrypto:
    def __init__(self, password):
        self.keys = [0x12345678, 0x23456789, 0x34567890]
        for byte in password:
            self._update(byte)

    def _update(self, byte):
        k0, k1, k2 = self.keys
        k0 = zlib.crc32(bytes((byte,)), k0 ^ 0xffffffff) ^ 0xffffffff
        k1 = ((k1 + (k0 & 0xff)) * 134775813 + 1) & 0xffffffff
        k2 = zlib.crc32(bytes((k1 >> 24,)), k2 ^ 0xffffffff) ^ 0xffffffff
        self.keys = [k0, k1, k2]

    def decrypt(self, data):
        out = bytearray(len(data))
        for i, byte in enumerate(data):
            temp = (self.keys[2] | 2) & 0xffff
            plain = byte ^ (((temp * (temp ^ 1)) >> 8) & 0xff)
            self._update(plain)
            out[i] = plain
        return bytes(out)


def scan_local_entries(data, password):
    """Fallback for TPFs whose central directory is damaged: walk local headers."""
    out, cursor = {}, 0
    while True:
        cursor = data.find(b'PK\x03\x04', cursor)
        if cursor < 0 or cursor + 30 > len(data):
            return out
        (_, _, flags, method, _, _, _, csize, usize, nlen, xlen) = struct.unpack_from('<IHHHHHIIIHH', data, cursor)
        name = data[cursor + 30:cursor + 30 + nlen].decode('cp437', 'replace')
        start = cursor + 30 + nlen + xlen
        blob = data[start:start + csize]
        if flags & 1:
            crypt = ZipCrypto(password)
            blob = crypt.decrypt(blob)[12:]
        if method == 8:
            blob = zlib.decompress(blob, -15)
        elif method != 0:
            raise ValueError(f'{name}: unsupported compression {method}')
        out[name] = blob
        cursor = start + max(csize, 1)


def read_tpf(path):
    """-> (entries {member: bytes}, def text, comment)."""
    data = tpf_unxor(Path(path).read_bytes())
    if data[:4] != b'PK\x03\x04':
        raise SystemExit(f'{path}: not a TexMod TPF (no zip signature after the XOR layer)')
    comment = ''
    try:
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            comment = archive.comment.decode('utf-8', 'replace')
            members = {info.filename: archive.read(info, pwd=TPF_PASSWORD) for info in archive.infolist()
                       if not info.is_dir()}
    except (zipfile.BadZipFile, RuntimeError, zlib.error, ValueError):
        members = scan_local_entries(data, TPF_PASSWORD)
    definition = next((members.pop(k) for k in list(members) if k.lower() == 'texmod.def'), None)
    if definition is None:
        raise SystemExit(f'{path}: texmod.def is missing')
    if definition[:2] in (b'\xff\xfe', b'\xfe\xff'):
        text = definition.decode('utf-16')
    else:
        text = definition.decode('utf-8', 'replace')
    return members, text.replace('\x00', ''), comment.strip()


def parse_texmod_def(text):
    """'0xKEY|member' lines -> [(key, member)]."""
    pairs = []
    for line in text.splitlines():
        line = line.strip()
        if not line or '|' not in line:
            continue
        key_text, member = line.split('|', 1)
        try:
            key = int(key_text.strip(), 16)
        except ValueError:
            continue
        if 0 <= key <= 0xffffffff:
            pairs.append((key, member.strip()))
    return pairs


# ---------------------------------------------------------------- images

def is_pow2(value):
    return value > 0 and not value & (value - 1)


def image_size(blob):
    if blob[:4] == b'DDS ' and len(blob) >= 128:
        height, width = struct.unpack_from('<II', blob, 12)
        return width, height, 'dds'
    if blob[:8] == b'\x89PNG\r\n\x1a\n' and len(blob) >= 24:
        width, height = struct.unpack_from('>II', blob, 16)
        return width, height, 'png'
    if blob[:2] == b'BM' and len(blob) >= 26:
        width, height = struct.unpack_from('<ii', blob, 18)
        return width, abs(height), 'bmp'
    if blob[:2] == b'\xff\xd8':
        return None, None, 'jpg'
    if len(blob) >= 18 and blob[2] in (2, 10):
        width, height = struct.unpack_from('<HH', blob, 12)
        return width, height, 'tga'
    return None, None, None


def write_dds_argb(image, path, mip_levels=0):
    """Pillow RGBA image -> uncompressed A8R8G8B8 DDS with a box-filtered mip chain
    (premultiplied while filtering, so transparent texels do not bleed)."""
    from PIL import Image
    rgba = image.convert('RGBA')
    width, height = rgba.size
    levels = []
    current = rgba
    while True:
        levels.append(current)
        if (current.width == 1 and current.height == 1) or (mip_levels and len(levels) >= mip_levels):
            break
        premultiplied = current.convert('RGBa')
        current = premultiplied.resize((max(1, current.width // 2), max(1, current.height // 2)),
                                       Image.Resampling.BOX).convert('RGBA')
    flags = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000 | (0x20000 if len(levels) > 1 else 0)
    caps = 0x1000 | (0x400008 if len(levels) > 1 else 0)
    header = struct.pack('<4sIIIIIII44sIIIIIIIIIIIII', b'DDS ', 124, flags, height, width, width * 4, 0,
                         len(levels), b'\0' * 44, 32, 0x41, 0, 32,
                         0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000, caps, 0, 0, 0, 0)
    with open(path, 'wb') as out:
        out.write(header)
        for level in levels:
            out.write(level.tobytes('raw', 'BGRA'))


# ---------------------------------------------------------------- packages

def ini_escape(value):
    return ' '.join(str(value).replace('\r', ' ').replace('\n', ' ').split())


def write_manifest(out_dir, meta, entries):
    """entries: [(key, relative path, comment)]."""
    lines = ['; SudekiMP texture mod package (docs/texture-mods.md)', '[Mod]', f'Format={FORMAT}']
    for field in ('Name', 'Version', 'Author', 'Description', 'Source'):
        if meta.get(field):
            lines.append(f'{field}={ini_escape(meta[field])}')
    lines += ['Enabled=true', '', '[Textures]',
              '; 0xKEY = file   (key: TexMod-compatible CRC-32 of the original level 0)']
    for key, relative, comment in sorted(entries, key=lambda e: e[1].lower()):
        if comment:
            lines.append(f'; {comment}')
        lines.append(f'0x{key:08X}={relative}')
    text = '\r\n'.join(lines) + '\r\n'
    manifest = Path(out_dir) / 'mod.ini'
    if all(ord(c) < 128 for c in text):
        manifest.write_bytes(text.encode('ascii'))
    else:  # GetPrivateProfileStringW reads UTF-16 files with a BOM
        manifest.write_bytes(b'\xff\xfe' + text.encode('utf-16-le'))


def read_manifest(mod_dir):
    raw = (Path(mod_dir) / 'mod.ini').read_bytes()
    text = raw[2:].decode('utf-16-le') if raw[:2] == b'\xff\xfe' else raw.decode('utf-8-sig')
    sections, current = {}, None
    for number, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if not stripped or stripped[0] in ';#':
            continue
        if stripped.startswith('[') and stripped.endswith(']'):
            current = sections.setdefault(stripped[1:-1].strip().lower(), [])
            continue
        if current is not None:
            current.append((number, stripped))
    return sections


SAFE_PATH = re.compile(r'^[^:*?"<>|\x00-\x1f]+$')


def safe_relative(path):
    parts = re.split(r'[\\/]', path)
    return bool(SAFE_PATH.match(path)) and path[0] not in '\\/' and all(p and p not in ('.', '..') for p in parts)


def safe_filename(name):
    return re.sub(r'[^A-Za-z0-9_.\-]+', '_', name).strip('._') or 'texture'


def prepare_output(out_dir, force):
    out = Path(out_dir)
    if out.exists() and any(out.iterdir()):
        if not force:
            raise SystemExit(f'{out} exists and is not empty (use --force to add to it)')
    (out / 'textures').mkdir(parents=True, exist_ok=True)
    return out


def store_texture(out, stem, blob, to_dds):
    """Write blob under textures/; with to_dds, re-encode as A8R8G8B8 DDS."""
    if to_dds and image_size(blob)[2] != 'dds':
        from PIL import Image
        relative = f'textures/{stem}.dds'
        write_dds_argb(Image.open(io.BytesIO(blob)), out / relative)
        return relative
    kind = image_size(blob)[2] or 'bin'
    relative = f'textures/{stem}.{kind}'
    (out / relative).write_bytes(blob)
    return relative


def load_catalog(args):
    if getattr(args, 'catalog', None):
        entries = []
        with open(args.catalog, encoding='ascii') as stream:
            next(stream)
            for line in stream:
                key, name, archive, archive_key, width, height, fmt = line.rstrip('\n').split('\t')
                entries.append({'key': int(key, 16), 'name': name, 'archive': archive,
                                'archive_key': int(archive_key, 16), 'width': int(width),
                                'height': int(height), 'format': fmt})
        return entries
    if getattr(args, 'game', None):
        return build_catalog(args.game, log=lambda m: print('catalog:', m, file=sys.stderr))
    return []


def command_convert_tpf(args):
    members, definition, comment = read_tpf(args.tpf)
    pairs = parse_texmod_def(definition)
    if not pairs:
        raise SystemExit('texmod.def lists no textures')
    by_key, _ = catalog_lookup(load_catalog(args))
    lower = {k.lower(): k for k in members}
    base = {Path(k.replace('\\', '/')).name.lower(): k for k in members}
    out = prepare_output(args.out, args.force)
    entries, missing, used = [], [], set()
    for key, member in pairs:
        name = lower.get(member.lower()) or base.get(Path(member.replace('\\', '/')).name.lower())
        if name is None:
            missing.append(member)
            continue
        known = by_key.get(key)
        stem = safe_filename(known['name']) if known and known['name'] else f'0x{key:08X}'
        while stem.lower() in used:
            stem += '_'
        used.add(stem.lower())
        relative = store_texture(out, stem, members[name], args.dds)
        note = f"{known['name'] or 'unnamed'} in {known['archive']} ({known['width']}x{known['height']} {known['format']})" if known else ''
        entries.append((key, relative, note))
    author, _, description = comment.partition('\n')
    meta = {'Name': args.name or Path(args.tpf).stem, 'Version': '1',
            'Author': author.strip(), 'Description': description.strip(),
            'Source': f'Converted from TexMod package {Path(args.tpf).name}'}
    write_manifest(out, meta, entries)
    print(f'{out}: {len(entries)} textures' + (f', {len(missing)} missing from the TPF: {missing}' if missing else ''))
    if by_key:
        named = sum(1 for e in entries if e[2])
        print(f'resolved {named}/{len(entries)} keys to game resources')
    return 1 if missing and not entries else 0


def command_catalog(args):
    entries = build_catalog(args.game, log=lambda m: print(m, file=sys.stderr))
    write_catalog(entries, args.out or '/dev/stdout')
    if args.out:
        print(f'{args.out}: {len(entries)} textures')
    return 0


def command_build(args):
    """SRC_DIR images named after the texture they replace: 'Verdana_16-0.tga.png'
    (archive resource name + image extension) or '0xAC57BC5D.png' (key)."""
    _, by_name = catalog_lookup(load_catalog(args))
    out = prepare_output(args.out, args.force)
    entries, unknown = [], []
    for path in sorted(Path(args.src).iterdir()):
        if path.suffix.lower() not in IMAGE_EXTENSIONS or not path.is_file():
            continue
        target = path.name[:-len(path.suffix)]
        if re.fullmatch(r'0x[0-9A-Fa-f]{1,8}', target):
            key, note = int(target, 16), ''
        elif target.upper() in by_name:
            known = by_name[target.upper()]
            key, note = known['key'], f"{known['name']} in {known['archive']} ({known['width']}x{known['height']} {known['format']})"
        else:
            unknown.append(path.name)
            continue
        relative = store_texture(out, safe_filename(target), path.read_bytes(), args.dds)
        entries.append((key, relative, note))
    meta = {'Name': args.name or Path(args.src).name, 'Version': args.version, 'Author': args.author,
            'Description': args.description}
    write_manifest(out, meta, entries)
    print(f'{out}: {len(entries)} textures')
    if unknown:
        print(f'not matched to a game texture (name them NAME.EXT.<img> or 0xKEY.<img>): {unknown}', file=sys.stderr)
    return 1 if unknown else 0


def command_validate(args):
    mod = Path(args.mod)
    problems, warnings = [], []
    if not (mod / 'mod.ini').is_file():
        raise SystemExit(f'{mod}: mod.ini missing')
    sections = read_manifest(mod)
    meta = dict(line.split('=', 1) for _, line in sections.get('mod', []) if '=' in line)
    fmt = meta.get('Format', '').strip()
    if not (fmt == FORMAT or fmt.startswith(FORMAT + '.')):
        problems.append(f'[Mod] Format must be {FORMAT} (found {fmt!r})')
    keys = {}
    for number, line in sections.get('textures', []):
        key_text, sep, relative = line.partition('=')
        key_text, relative = key_text.strip(), relative.strip()
        if not sep or not re.fullmatch(r'0[xX][0-9A-Fa-f]{1,8}', key_text):
            problems.append(f'line {number}: expected 0xKEY=file, got {line!r}')
            continue
        if not safe_relative(relative):
            problems.append(f'line {number}: unsafe path {relative!r}')
            continue
        key = int(key_text, 16)
        if key in keys:
            warnings.append(f'line {number}: 0x{key:08X} repeats line {keys[key]} (the later line wins)')
        keys[key] = number
        path = mod / relative.replace('\\', '/')
        if not path.is_file():
            problems.append(f'line {number}: {relative} does not exist')
            continue
        with path.open("rb") as stream:
            width, height, kind = image_size(stream.read(128))
        if kind is None:
            problems.append(f'line {number}: {relative} is not a DDS/PNG/TGA/BMP/JPG image')
        elif width and height and not (is_pow2(width) and is_pow2(height)):
            warnings.append(f'line {number}: {relative} is {width}x{height}; it will be resampled to a power of two')
    for w in warnings:
        print('warning:', w)
    for p in problems:
        print('error:', p)
    print(f"{mod}: {len(keys)} textures, {len(problems)} errors, {len(warnings)} warnings")
    return 1 if problems else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = parser.add_subparsers(dest='command', required=True)
    p = sub.add_parser('convert-tpf', help='convert a TexMod .tpf into a SudekiMP mod folder')
    p.add_argument('tpf')
    p.add_argument('out')
    p.add_argument('--game', help='Sudeki folder; names textures after their archive resources')
    p.add_argument('--catalog', help='catalog TSV from the catalog command (instead of --game)')
    p.add_argument('--name')
    p.add_argument('--dds', action='store_true', help='re-encode images as A8R8G8B8 DDS with mipmaps (Pillow)')
    p.add_argument('--force', action='store_true')
    p.set_defaults(run=command_convert_tpf)
    p = sub.add_parser('catalog', help='list every TGA/SQX texture with its key and resource name')
    p.add_argument('--game', required=True)
    p.add_argument('--out')
    p.set_defaults(run=command_catalog)
    p = sub.add_parser('build', help='build a mod from images named NAME.EXT.<img> or 0xKEY.<img>')
    p.add_argument('src')
    p.add_argument('out')
    p.add_argument('--game')
    p.add_argument('--catalog')
    p.add_argument('--name')
    p.add_argument('--version', default='1')
    p.add_argument('--author', default='')
    p.add_argument('--description', default='')
    p.add_argument('--dds', action='store_true')
    p.add_argument('--force', action='store_true')
    p.set_defaults(run=command_build)
    p = sub.add_parser('validate', help='check a mod folder')
    p.add_argument('mod')
    p.set_defaults(run=command_validate)
    args = parser.parse_args(argv)
    return args.run(args)


if __name__ == '__main__':
    sys.exit(main())
