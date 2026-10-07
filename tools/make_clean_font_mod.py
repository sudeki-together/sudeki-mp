#!/usr/bin/env python3
"""Example texture mod: redraw Sudeki's HUD/dialogue font sheets cleanly at 4x.

Reads YOUR copy of Fonts.baf (read-only), finds every font sheet whose layout
matches a glyph descriptor in the archive, and redraws each glyph from a
TrueType font at SCALE times the resolution, in the same cell, with the
game's own style: flat 0xEA fill, a one-pixel bottom-right 0xC2 extrusion and a
0x20 outline two pixels wide and one pixel tall (all scaled). The game keeps
using its original glyph table, so only the pixels get sharper.

Output is a SudekiMP texture mod folder (docs/texture-mods.md):
    OUT/mod.ini, OUT/textures/<sheet>.dds   (A8R8G8B8 with mipmaps)
Copy OUT into the game's mods folder. The generated sheets are derived from
the game's glyph layout and from the font you supply, so they are produced on
your machine and never committed or redistributed by this project.

Requires numpy and Pillow, and a font file you are licensed to use:
    python3 tools/make_clean_font_mod.py --game "C:/GOG Games/Sudeki" \\
        --font /path/to/Verdana.ttf --out mods/CleanFont4x [--preview cmp.png]
The sheets were drawn from Verdana (descriptor face name); another sans font
works but changes the letter shapes.
"""
import argparse
import io
import mmap
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sudekimod  # noqa: E402

FILL, EXTRUDE, OUTLINE = 0xEA, 0xC2, 0x20
DESCRIPTOR_MAGIC = b'\x01\x00\x00\x00\x06\x00\x00\x00'


def parse_descriptor(blob):
    """Glyph descriptor: LOGFONT-like header, kerning pairs, 14-byte glyph records.

    0x08 i16 lfHeight, 0x10 u32 weight, 0x1A char[32] face, 0x3C u32 line
    height, 0x4A u16 last char, 0x4C u16 glyph count, 0x4E u16 kerning count,
    0x54 kerning {u16 first, u16 second, i16 amount}, then glyphs
    {u16 char, u16 page, u16 x, u16 y, u8 w, u8 h, u8 0, i8 a, u8 advance, i8 c}.
    Both Fonts.baf descriptors parse to exactly their resource size."""
    if blob[:8] != DESCRIPTOR_MAGIC or len(blob) < 0x54:
        return None
    height = struct.unpack_from('<h', blob, 8)[0]
    face = blob[0x1a:0x3a].split(b'\0')[0].decode('ascii', 'replace')
    glyph_count, kern_count = struct.unpack_from('<HH', blob, 0x4c)
    start = 0x54 + kern_count * 6
    if start + glyph_count * 14 != len(blob):
        return None
    glyphs = []
    for i in range(glyph_count):
        char, page, x, y, w, h, _, a, advance, c = struct.unpack_from('<HHHHBBbbBb', blob, start + i * 14)
        # Pen advance is a + w + c (matches Verdana's advance widths at this size).
        glyphs.append({'char': char, 'page': page, 'x': x, 'y': y, 'w': w, 'h': h, 'a': a, 'c': c})
    return {'face': face, 'height': height, 'glyphs': glyphs}


def load_fonts_archive(game):
    path = next((p for p in Path(game).iterdir() if p.name.lower() == 'fonts.baf'), None)
    if path is None:
        raise SystemExit(f'Fonts.baf not found in {game}')
    with path.open('rb') as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        return {key: bytes(data[o:o + s]) for o, s, key in sudekimod.index_archive(data)}


def match_sheets(resources):
    """[(archive key, sheet pixels HxWx4 BGRA top-down, descriptor)] for sheets
    with >= 99.5% of their alpha inside one descriptor's glyph cells."""
    descriptors = [d for d in (parse_descriptor(b) for b in resources.values()) if d]
    out = []
    for key, blob in sorted(resources.items()):
        level = sudekimod.tga_level0(blob)
        if not level:
            continue
        width, height, pixels = level
        sheet = np.frombuffer(pixels, np.uint8).reshape(height, width, 4)
        alpha = sheet[:, :, 3].astype(np.float64)
        best, best_score = None, 0.0
        for descriptor in descriptors:
            mask = np.zeros(alpha.shape, bool)
            for g in descriptor['glyphs']:
                mask[g['y']:g['y'] + g['h'], g['x']:g['x'] + g['w']] = True
            score = alpha[mask].sum() / max(alpha.sum(), 1.0)
            if score > best_score:
                best, best_score = descriptor, score
        if best and best_score >= 0.995:
            out.append((key, sheet, best, pixels))
    return out


def sheet_name(archive_key, descriptor):
    size = {-21: 16, -24: 18}.get(descriptor['height'])
    for candidate in ([f"{descriptor['face']}_{size}-{p}.tga" for p in range(4)] if size else []):
        if sudekimod.resource_checksum(candidate) == archive_key:
            return candidate
    return None


def dilate(mask, offsets):
    """Grey-level dilation by a set of (dy, dx) offsets (max filter)."""
    out = np.zeros_like(mask)
    h, w = mask.shape
    for dy, dx in offsets:
        ys, yd = (slice(0, h - dy), slice(dy, h)) if dy >= 0 else (slice(-dy, h), slice(0, h + dy))
        xs, xd = (slice(0, w - dx), slice(dx, w)) if dx >= 0 else (slice(-dx, w), slice(0, w + dx))
        np.maximum(out[yd, xd], mask[ys, xs], out=out[yd, xd])
    return out


def style_offsets(scale):
    sweep = [(dy, dx) for dy in range(scale + 1) for dx in range(scale + 1)]
    rx, ry = 2 * scale, scale
    ellipse = [(dy, dx) for dy in range(-ry, ry + 1) for dx in range(-rx, rx + 1)
               if (dx / (rx + 0.5)) ** 2 + (dy / (ry + 0.5)) ** 2 <= 1.0]
    return sweep, ellipse


def styled_glyph(glyph_mask, sweep, ellipse):
    """glyph coverage (0..1) -> (rgb grey, alpha) with fill/extrusion/outline."""
    body = dilate(glyph_mask, sweep)
    alpha = np.maximum(dilate(body, ellipse), body)
    grey = OUTLINE + (EXTRUDE - OUTLINE) * body
    grey = grey + (FILL - EXTRUDE) * glyph_mask
    return grey, alpha, body


def original_body(cell):
    """Coverage of fill+extrusion in an original 1x cell (outline excluded)."""
    grey = cell[:, :, 2].astype(np.float64)
    alpha = cell[:, :, 3].astype(np.float64) / 255.0
    return np.clip((grey - OUTLINE) / (EXTRUDE - OUTLINE), 0, 1) * (alpha > 0.99)


def downsample(array, scale):
    h, w = array.shape[0] // scale, array.shape[1] // scale
    return array[:h * scale, :w * scale].reshape(h, scale, w, scale).mean(axis=(1, 3))


def render_mask(font, char):
    try:
        mask, offset = font.getmask2(chr(char), mode='L')
    except (UnicodeEncodeError, OSError, ValueError):
        return None
    if mask.size[0] == 0 or mask.size[1] == 0:
        return None
    image = Image.frombytes('L', mask.size, bytes(mask))
    box = image.getbbox()  # ink only; the mask also spans the side bearings
    return np.asarray(image.crop(box), np.float64) / 255.0 if box else None


def draw_sheet(sheet, descriptor, font, scale, notdef, log):
    height, width = sheet.shape[:2]
    out = np.zeros((height * scale, width * scale, 4), np.float64)  # grey, alpha in [0,255],[0,1]
    out[:, :, 0] = OUTLINE
    sweep, ellipse = style_offsets(scale)
    fallback = 0
    for g in descriptor['glyphs']:
        x, y = g['x'], g['y']
        w, h = min(g['w'], width - x), min(g['h'], height - y)  # some cells run off the sheet
        if w <= 0 or h <= 0:
            continue
        cell = sheet[y:y + h, x:x + w]
        if not cell[:, :, 3].any():
            continue
        target = original_body(cell)
        mask = render_mask(font, g['char'])
        if mask is None or (notdef is not None and mask.shape == notdef.shape and np.array_equal(mask, notdef)):
            mask = None
        placed = None
        if mask is not None and target.any():
            pad = 3 * scale
            canvas_h, canvas_w = h * scale, w * scale
            padded = np.pad(mask, ((0, scale), (0, scale)))
            body = dilate(padded, sweep)  # fill + extrusion, translated during the search
            bh, bw = body.shape
            ty, tx = np.nonzero(target)
            # Start from the body bounding-box centre, refine by L1 on the 1x body.
            cy = int(round((ty.min() + ty.max() + 1) / 2 * scale - bh / 2))
            cx = int(round((tx.min() + tx.max() + 1) / 2 * scale - bw / 2))
            best = None
            for oy in range(cy - pad, cy + pad + 1):
                for ox in range(cx - pad, cx + pad + 1):
                    if oy < 0 or ox < 0 or oy + bh > canvas_h or ox + bw > canvas_w:
                        continue
                    trial = np.zeros((canvas_h, canvas_w))
                    trial[oy:oy + bh, ox:ox + bw] = body
                    error = np.abs(downsample(trial, scale) - target).sum()
                    if best is None or error < best[0]:
                        best = (error, oy, ox)
            if best is not None and best[0] <= 0.5 * target.sum() + 2.0:
                placed = np.zeros((canvas_h, canvas_w))
                placed[best[1]:best[1] + bh, best[2]:best[2] + bw] = padded
        if placed is None:
            # Missing glyph or poor fit: keep the original cell, smoothly enlarged.
            fallback += 1
            big = Image.fromarray(cell[:, :, [2, 1, 0, 3]]).resize((w * scale, h * scale), Image.Resampling.LANCZOS)
            big = np.asarray(big, np.float64)
            out[y * scale:(y + h) * scale, x * scale:(x + w) * scale, 0] = big[:, :, 0]
            out[y * scale:(y + h) * scale, x * scale:(x + w) * scale, 1] = big[:, :, 3] / 255.0
            continue
        grey, alpha, _ = styled_glyph(placed, sweep, ellipse)
        region = out[y * scale:(y + h) * scale, x * scale:(x + w) * scale]
        region[:, :, 0] = grey
        region[:, :, 1] = np.clip(alpha, 0, 1)
    log(f"  {len(descriptor['glyphs'])} glyphs, {fallback} kept from the original (missing in the font or poor fit)")
    grey = np.clip(np.rint(out[:, :, 0]), 0, 255).astype(np.uint8)
    alpha = np.clip(np.rint(out[:, :, 1] * 255), 0, 255).astype(np.uint8)
    return Image.fromarray(np.dstack([grey, grey, grey, alpha]), 'RGBA')


def render_text(sheet_image, descriptor, text, sheet_scale, zoom):
    """Lay out text with the game's glyph table and sample the sheet bilinearly
    at zoom x the original size, as the GPU does when the HUD is scaled up."""
    glyphs = {g['char']: g for g in descriptor['glyphs']}
    line = max(g['h'] for g in descriptor['glyphs'])
    width = sum(glyphs[ord(ch)]['a'] + glyphs[ord(ch)]['w'] + glyphs[ord(ch)]['c'] for ch in text if ord(ch) in glyphs)
    canvas = Image.new('RGBA', (int((width + 8) * zoom), int((line + 4) * zoom)), (70, 62, 52, 255))
    pen = 4.0
    for ch in text:
        g = glyphs.get(ord(ch))
        if not g:
            continue
        box = tuple(v * sheet_scale for v in (g['x'], g['y'], g['x'] + g['w'], g['y'] + g['h']))
        cell = sheet_image.crop(box).resize((max(1, round(g['w'] * zoom)), max(1, round(g['h'] * zoom))),
                                            Image.Resampling.BILINEAR, reducing_gap=None)
        canvas.alpha_composite(cell, (int(round((pen + g['a']) * zoom)), int(2 * zoom)))
        pen += g['a'] + g['w'] + g['c']
    return canvas


def preview(original_pixels, size, new_image, descriptor, scale, path):
    """Sample text drawn from the original sheet (top) and the new one (bottom)."""
    width, height = size
    original = Image.frombuffer('RGBA', (width, height), original_pixels, 'raw', 'BGRA', 0, 1)
    rows = []
    for text in ('Ailish 400 HP 36 SP', 'Gr\u00f6\u00dfe, \u00e9t\u00e9, \u00bd \u20ac \u2122  \u041f\u0440\u0438\u0432\u0435\u0442!'):
        rows += [render_text(original, descriptor, text, 1, 3.0), render_text(new_image, descriptor, text, scale, 3.0)]
    canvas = Image.new('RGB', (max(r.width for r in rows), sum(r.height + 6 for r in rows)), (24, 24, 28))
    y = 0
    for row in rows:
        canvas.paste(row.convert('RGB'), (0, y))
        y += row.height + 6
    canvas.save(path)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--game', required=True, help='Sudeki folder containing Fonts.baf')
    parser.add_argument('--font', required=True, help='TrueType font to draw from (Verdana for the original look)')
    parser.add_argument('--out', required=True, help='new mod folder')
    parser.add_argument('--scale', type=int, default=4, choices=(2, 3, 4))
    parser.add_argument('--preview', help='write a PNG comparing sample text from the first sheet')
    parser.add_argument('--force', action='store_true')
    args = parser.parse_args(argv)
    resources = load_fonts_archive(args.game)
    sheets = match_sheets(resources)
    if not sheets:
        raise SystemExit('no font sheet in Fonts.baf matches a glyph descriptor')
    out = sudekimod.prepare_output(args.out, args.force)
    entries, done = [], set()
    for number, (archive_key, sheet, descriptor, pixels) in enumerate(sheets):
        name = sheet_name(archive_key, descriptor)
        key = sudekimod.texmod_key(pixels)
        if key in done:  # identical sheets share one key (and one replacement)
            print(f'archive key 0x{archive_key:08X}: same pixels as an earlier sheet (key 0x{key:08X})')
            continue
        done.add(key)
        label = name or f'archive key 0x{archive_key:08X}'
        print(f"{label}: {descriptor['face']} {descriptor['height']}px, key 0x{key:08X}")
        font = ImageFont.truetype(args.font, abs(descriptor['height']) * args.scale)
        notdef = render_mask(font, 0xFFFF)
        image = draw_sheet(sheet, descriptor, font, args.scale, notdef, print)
        stem = sudekimod.safe_filename(name) if name else f'0x{key:08X}'
        relative = f'textures/{stem}.dds'
        sudekimod.write_dds_argb(image, out / relative)
        entries.append((key, relative, f'{label} in Fonts.baf ({sheet.shape[1]}x{sheet.shape[0]} -> '
                                        f'{image.width}x{image.height})'))
        if args.preview and number == 0:
            preview(pixels, (sheet.shape[1], sheet.shape[0]), image, descriptor, args.scale, args.preview)
    sudekimod.write_manifest(out, {
        'Name': f'Clean {args.scale}x Font', 'Version': '1', 'Author': 'SudekiMP example',
        'Description': f'Every Fonts.baf glyph sheet redrawn at {args.scale}x from {Path(args.font).name} '
                       'in the original style; glyph layout unchanged.',
        'Source': 'Generated locally by tools/make_clean_font_mod.py'}, entries)
    print(f'{out}: {len(entries)} sheets')
    return 0


if __name__ == '__main__':
    sys.exit(main())
