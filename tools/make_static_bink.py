#!/usr/bin/env python3
"""Write a rainbow-static Bink 1 ('BIKi') video, for movie mods and tests.

A deliberately minimal, intra-only Bink 1 writer: every 8x8 block of every
plane is a two-colour PATTERN block (per-pixel bit pattern), every Huffman
tree is the identity 4-bit tree, there is no audio track. Luma gives
per-pixel static; chroma gives per-2x2 rainbow confetti whose hues drift.

Layout follows the public FFmpeg Bink demuxer and decoder (libavformat/bink.c,
libavcodec/bink.c): 0x2C-byte header, frames+1 index entries (bit 0 =
keyframe), then frames. A 'BIKi' frame starts with 32 bits that the
original movies set to the byte offset of the first chroma plane; each plane
is 9 bundle headers, then per block row the bundle data, padded to 32 bits.
Verify with: ffprobe out.bik ; ffmpeg -i out.bik frame%03d.png

    python3 tools/make_static_bink.py out.bik [--width 1920 --height 1080
        --fps 30 --seconds 4 --seed 1]
Requires numpy. No RAD Game Tools code or data is used.
"""
import argparse
import math
import struct
import sys

import numpy as np

PATTERN_BLOCK = 8


def av_log2(value):
    return int(math.floor(math.log2(value)))


class BitWriter:
    """Little-endian bit writer (first bit = least significant bit)."""

    def __init__(self):
        self.values, self.lengths = [], []

    def put(self, value, bits):
        self.values.append(np.asarray([value], np.uint32))
        self.lengths.append(np.asarray([bits], np.uint8))

    def put_array(self, values, bits):
        values = np.asarray(values, np.uint32).ravel()
        self.values.append(values)
        self.lengths.append(np.full(values.size, bits, np.uint8))

    def bit_count(self):
        return int(sum(int(l.astype(np.int64).sum()) for l in self.lengths))

    def align32(self):
        pad = -self.bit_count() % 32
        if pad:
            self.put(0, pad if pad <= 16 else 16)
            if pad > 16:
                self.put(0, pad - 16)

    def bytes(self):
        values = np.concatenate(self.values)
        lengths = np.concatenate(self.lengths).astype(np.int64)
        total = int(lengths.sum())
        owner = np.repeat(np.arange(values.size), lengths)
        start = np.repeat(np.cumsum(lengths) - lengths, lengths)
        shift = (np.arange(total) - start).astype(np.uint32)
        bits = ((values[owner] >> shift) & 1).astype(np.uint8)
        return np.packbits(bits, bitorder='little').tobytes()


def encode_plane(out, colors, patterns, plane_width, blocks_wide, blocks_high):
    """colors: (bh, bw, 2) uint8; patterns: (bh, bw, 8) uint8 (bit j = pixel j)."""
    width = max(plane_width, 8)
    width = (width + 7) & ~7
    len_types = av_log2((width >> 3) + 511) + 1
    len_sub = av_log2((width >> 4) + 511) + 1
    len_colors = av_log2(blocks_wide * 64 + 511) + 1
    len_pattern = av_log2((blocks_wide << 3) + 511) + 1
    len_run = av_log2(blocks_wide * 48 + 511) + 1
    # Bundle headers: identity tree (vlc 0) everywhere; colours also carry 16
    # high-nibble trees; the two DC bundles have none.
    for source in range(9):
        if source == 2:
            for _ in range(16):
                out.put(0, 4)
        if source not in (6, 7):
            out.put(0, 4)
    for by in range(blocks_high):
        out.put(blocks_wide, len_types)       # block types: a fill run of PATTERN
        out.put(1, 1)
        out.put(PATTERN_BLOCK, 4)
        if by == 0:
            out.put(0, len_sub)               # 16x16 sub-block types: none (stops reading)
        values = colors[by].ravel()
        out.put(values.size, len_colors)
        out.put(0, 1)                         # coded, not a fill
        nibbles = np.empty(values.size * 2, np.uint32)
        nibbles[0::2] = values >> 4           # high nibble (col_high tree) first
        nibbles[1::2] = values & 15
        out.put_array(nibbles, 4)
        values = patterns[by].ravel()
        out.put(values.size, len_pattern)
        nibbles = np.empty(values.size * 2, np.uint32)
        nibbles[0::2] = values & 15           # pattern: low nibble first
        nibbles[1::2] = values >> 4
        out.put_array(nibbles, 4)
        if by == 0:
            out.put(0, len_types)             # x offsets: none
            out.put(0, len_types)             # y offsets: none
            out.put(0, len_types)             # intra DC: none
            out.put(0, len_types)             # inter DC: none
            out.put(0, len_run)               # runs: none
    out.align32()


def hsv_to_yuv(hue):
    """Fully saturated hue (0..1) -> (U, V), BT.601 video range."""
    r = np.clip(np.abs(hue * 6 - 3) - 1, 0, 1)
    g = np.clip(2 - np.abs(hue * 6 - 2), 0, 1)
    b = np.clip(2 - np.abs(hue * 6 - 4), 0, 1)
    u = 128 + 224 * (-0.168736 * r - 0.331264 * g + 0.5 * b)
    v = 128 + 224 * (0.5 * r - 0.418688 * g - 0.081312 * b)
    return np.clip(np.rint(u), 16, 240).astype(np.uint8), np.clip(np.rint(v), 16, 240).astype(np.uint8)


def make_frame(rng, width, height, frame, frames):
    out = BitWriter()
    out.put(0, 16)  # placeholder for the chroma offset, patched after Y
    out.put(0, 16)
    bw, bh = (width + 7) >> 3, (height + 7) >> 3
    luma_colors = np.stack([rng.integers(16, 70, (bh, bw)), rng.integers(170, 236, (bh, bw))], -1).astype(np.uint8)
    encode_plane(out, luma_colors, rng.integers(0, 256, (bh, bw, 8), dtype=np.uint8), width, bw, bh)
    chroma_offset = out.bit_count() // 8
    cbw, cbh = (width + 15) >> 4, (height + 15) >> 4
    drift = frame / max(frames, 1)
    for plane in range(2):  # stored order: V then U ('h' and later swap planes)
        hues = (rng.random((cbh, cbw, 2)) + drift) % 1.0
        u, v = hsv_to_yuv(hues)
        encode_plane(out, v if plane == 0 else u, rng.integers(0, 256, (cbh, cbw, 8), dtype=np.uint8),
                     width >> 1, cbw, cbh)
    data = bytearray(out.bytes())
    struct.pack_into('<I', data, 0, chroma_offset)
    return bytes(data)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('out')
    parser.add_argument('--width', type=int, default=1920)
    parser.add_argument('--height', type=int, default=1080)
    parser.add_argument('--fps', type=int, default=30)
    parser.add_argument('--seconds', type=float, default=4.0)
    parser.add_argument('--seed', type=int, default=1)
    args = parser.parse_args(argv)
    if not (16 <= args.width <= 7680 and 16 <= args.height <= 4800 and 1 <= args.fps <= 120):
        raise SystemExit('unsupported size or rate')
    frames = max(1, int(round(args.seconds * args.fps)))
    rng = np.random.default_rng(args.seed)
    payloads = []
    for i in range(frames):
        payloads.append(make_frame(rng, args.width, args.height, i, frames))
        if i % 30 == 0:
            print(f'frame {i + 1}/{frames}', file=sys.stderr)
    header_size = 0x2C + 4 * (frames + 1)
    offsets, at = [], header_size
    for payload in payloads:
        offsets.append(at)
        at += len(payload)
    file_size = at
    header = struct.pack('<4sIIIIIIIIII', b'BIKi', file_size - 8, frames, max(map(len, payloads)), frames,
                         args.width, args.height, args.fps, 1, 0, 0)
    index = b''.join(struct.pack('<I', o | 1) for o in offsets) + struct.pack('<I', file_size)
    with open(args.out, 'wb') as stream:
        stream.write(header + index)
        for payload in payloads:
            stream.write(payload)
    print(f'{args.out}: {frames} frames {args.width}x{args.height} @ {args.fps} fps, {file_size} bytes')
    return 0


if __name__ == '__main__':
    sys.exit(main())
