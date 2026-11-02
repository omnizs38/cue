#!/usr/bin/env python3
"""Draws res/cue.ico (Cue's pixel play mark on a dark rounded tile, also the icon of video
files) and res/cue-audio.ico (the same tile with a pixel note, for music), in seven sizes each.

    python3 tools/gen_icon.py            writes both icons (requires Pillow)

Every size is drawn on its own grid rather than scaled from one master, so the play
mark stays pixel-exact. The tile fills the whole icon box and carries a light rim, so
its silhouette remains visible on dark taskbars; the mark is about 58% of the tile.
"""
import os
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIZES = [16, 24, 32, 48, 64, 128, 256]
RIM, FILL, INK = (78, 78, 78), (8, 8, 8), (255, 255, 255)


def tile(size, mark=0.58):
    ss = 8  # the rounded outline is supersampled; the mark is drawn on exact pixels
    big = size * ss
    im = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    inset = 0 if size <= 32 else size // 48
    radius = int(big * 0.22)
    o = inset * ss
    rim = ss if size <= 32 else max(ss, big // 64)  # 1 px at small sizes, a hairline on large ones
    d.rounded_rectangle([o, o, big - 1 - o, big - 1 - o], radius, fill=RIM + (255,))
    d.rounded_rectangle([o + rim, o + rim, big - 1 - o - rim, big - 1 - o - rim], max(1, radius - rim), fill=FILL + (255,))
    im = im.resize((size, size), Image.LANCZOS)
    # The play mark in the interface's proportion: each column is two cells shorter.
    unit = max(1, round(size / 24)) if size < 64 else size // 32
    rows = int(round(size * mark / unit)) // 2 * 2
    cols = rows // 2
    x0 = (size - cols * unit) // 2 + unit // 2 + (unit // 2 if size >= 48 else 0)
    y0 = (size - rows * unit) // 2
    px = im.load()
    for c in range(cols):
        for r in range(c, rows - c):
            for dx in range(unit):
                for dy in range(unit):
                    px[x0 + c * unit + dx, y0 + r * unit + dy] = INK + (255,)
    return im


# The music file icon: the same tile, with a pixel note where the play mark would be (video
# files simply carry Cue's own icon). The note is drawn cell by cell, about as tall as the mark.
NOTE = ["...###..", "...#.##.", "...#..##", "...#...#", "...#....", "...#....", ".###....", "####....", "####....", ".##....."]


def music_icon(size):
    im = tile(size, mark=0)
    rows, cols = len(NOTE), len(NOTE[0])
    unit = max(1, round(size * 0.58 / rows))
    x0, y0 = (size - cols * unit) // 2, (size - rows * unit) // 2
    px = im.load()
    for r, row in enumerate(NOTE):
        for c, ch in enumerate(row):
            if ch == "#":
                for dy in range(unit):
                    for dx in range(unit):
                        px[x0 + c * unit + dx, y0 + r * unit + dy] = INK + (255,)
    return im


def main():
    images = [tile(s) for s in SIZES]
    out = os.path.join(ROOT, "res", "cue.ico")
    images[-1].save(out, sizes=[(s, s) for s in SIZES], append_images=images[:-1])
    print("wrote", os.path.relpath(out, ROOT))
    images = [music_icon(s) for s in SIZES]
    out = os.path.join(ROOT, "res", "cue-audio.ico")
    images[-1].save(out, sizes=[(s, s) for s in SIZES], append_images=images[:-1])
    print("wrote", os.path.relpath(out, ROOT))


if __name__ == "__main__":
    main()
