#!/usr/bin/env python3
"""Validate mod_tileset.json sprite indices against the actual spritesheets.

Sprite indices in a mod_tileset are GLOBAL across the whole file, not per-image:
each block continues numbering where the previous block ended, and a block
reserves cells based on its image's dimensions, not on how many cells contain
art.  Getting this wrong produces no error message at all - monsters silently
render the wrong sprite, or nothing at all.

Runs from anywhere - it locates the tileset relative to its own file:

    python tools/check_sprites.py              # validate
    python tools/check_sprites.py --occupancy  # also map which cells have art
    python tools/check_sprites.py --tileset X  # point it somewhere else

Exits non-zero if anything is wrong.  Pure stdlib; no Pillow needed.
"""
from __future__ import annotations

import argparse
import json
import os
import struct
import sys
import zlib


def png_size(path: str) -> tuple[int, int]:
    """Width and height from the IHDR chunk."""
    with open(path, "rb") as fh:
        head = fh.read(33)
    if head[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path} is not a PNG")
    width, height = struct.unpack(">II", head[16:24])
    return width, height


def png_rgba(path: str) -> tuple[int, int, bytes]:
    """Decode an 8-bit RGBA PNG to raw pixels. Only used for --occupancy."""
    data = open(path, "rb").read()
    pos, idat, width, height, depth, ctype = 8, b"", 0, 0, 0, 0
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        tag = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        if tag == b"IHDR":
            width, height, depth, ctype = struct.unpack(">IIBB", chunk[:10])
        elif tag == b"IDAT":
            idat += chunk
        elif tag == b"IEND":
            break
        pos += 12 + length
    if (depth, ctype) != (8, 6):
        raise ValueError(f"{path}: expected 8-bit RGBA, got depth={depth} colortype={ctype}")

    raw = zlib.decompress(idat)
    bpp, stride = 4, width * 4
    out = bytearray(height * stride)
    prev = bytearray(stride)
    p = 0
    for y in range(height):
        filt = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if filt == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif filt == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif filt == 3:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif filt == 4:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                c = prev[i - bpp] if i >= bpp else 0
                b = prev[i]
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
        elif filt != 0:
            raise ValueError(f"{path}: bad filter {filt} on row {y}")
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return width, height, bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    # Default resolves against this script's own location rather than the cwd, so
    # the check works from anywhere.  tools/ sits inside the mod folder, so the
    # tileset is one level up.
    here = os.path.dirname(os.path.abspath(__file__))
    default_tileset = os.path.join(here, os.pardir, "mod_tileset.json")
    ap.add_argument("--tileset", default=os.path.normpath(default_tileset))
    ap.add_argument("--occupancy", action="store_true",
                    help="also report which cells of each sheet contain art")
    args = ap.parse_args()

    if not os.path.exists(args.tileset):
        print(f"ERROR: {args.tileset} not found - pass --tileset to override",
              file=sys.stderr)
        return 2

    doc = json.load(open(args.tileset, encoding="utf-8"))
    problems: list[str] = []
    base = 0

    print(f"{'sheet':44s} {'cell':>8s} {'cells':>6s} {'sprite range':>15s}")
    print("-" * 78)

    # Sheet filenames in a mod_tileset are relative to the tileset file itself,
    # not to wherever this was run from.  Resolve them that way so the check
    # works from any cwd.
    sheet_dir = os.path.dirname(os.path.abspath(args.tileset))

    for block in doc[0]["tiles-new"]:
        name = block["file"]
        path = os.path.join(sheet_dir, name)
        sw, sh = block["sprite_width"], block["sprite_height"]

        if not os.path.exists(path):
            problems.append(f"{name}: file missing")
            continue

        iw, ih = png_size(path)
        if iw % sw or ih % sh:
            problems.append(
                f"{name}: {iw}x{ih} is not an exact multiple of {sw}x{sh} - "
                "the engine asserts on this")
        cols, rows = iw // sw, ih // sh
        count = cols * rows
        lo, hi = base, base + count - 1

        print(f"{name:44s} {sw:>3d}x{sh:<4d} {count:6d} {lo:7d}-{hi:<7d}")

        seen: dict[int, str] = {}
        for tile in block["tiles"]:
            # "fg" may be a bare sprite index or a list of weighted entries.
            # Both are valid tileset syntax; normalise before walking.
            fg = tile["fg"]
            for entry in (fg if isinstance(fg, list) else [fg]):
                sprite = entry["sprite"] if isinstance(entry, dict) else entry
                if not lo <= sprite <= hi:
                    problems.append(
                        f"{tile['id']}: sprite {sprite} is outside this block's range "
                        f"{lo}-{hi}.  Indices are global across the whole tileset, not "
                        f"per-file - local cell N is sprite {lo}+N.")
                else:
                    seen[sprite] = tile["id"]

        if args.occupancy:
            _, _, px = png_rgba(path)
            unreferenced = []
            for r in range(rows):
                for c in range(cols):
                    idx = base + r * cols + c
                    opaque = 0
                    for y in range(r * sh, (r + 1) * sh):
                        row = y * iw * 4 + c * sw * 4
                        for x in range(sw):
                            if px[row + x * 4 + 3]:
                                opaque += 1
                    if idx in seen and opaque < 30:
                        problems.append(
                            f"{seen[idx]}: sprite {idx} is referenced but nearly empty "
                            f"({opaque} opaque px) - stray pixel, or wrong index?")
                    elif idx not in seen and opaque >= 30:
                        unreferenced.append(idx)
            if unreferenced:
                shown = ", ".join(str(i) for i in unreferenced[:20])
                more = " ..." if len(unreferenced) > 20 else ""
                print(f"{'':44s} art not referenced by any monster: {shown}{more}")

        base += count

    print()
    if problems:
        print(f"FAILED - {len(problems)} problem(s):", file=sys.stderr)
        for problem in problems:
            print(f"  * {problem}", file=sys.stderr)
        return 1

    print("OK - every sprite reference falls inside its own block's range.")
    print()
    print("Reminder: cells are reserved by CANVAS SIZE, and blocks are numbered in FILE ORDER.")
    print("  Filling empty cells of an existing sheet is safe - nothing shifts.")
    print("  Resizing a sheet, or inserting a block before the end, renumbers everything after it.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
