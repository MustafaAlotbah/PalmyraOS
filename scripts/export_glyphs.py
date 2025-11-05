#!/usr/bin/env python3
# python export_glyphs.py --font ./FiraCode-Light.ttf --size 12
# python export_glyphs.py --font ./FiraCode-Light.ttf --size 12 --range 0x20-0x7E
# python export_glyphs.py --font ./FiraCode-Light.ttf --size 14 --family FiraCode --out firacode_14px.txt



from pathlib import Path
from math import ceil
from typing import Iterable
from PIL import Image, ImageDraw, ImageFont
import numpy as np
import argparse
import sys

def load_font(font_path: str, px: int) -> ImageFont.FreeTypeFont:
    try:
        return ImageFont.truetype(font_path, px)
    except OSError as e:
        sys.exit(f"Failed to load font '{font_path}': {e}")

def col_to_hex(col_bits: np.ndarray) -> str:
    # Pack top->bottom into a single integer and return hex string with 0x prefix
    s = ''.join('1' if int(b) else '0' for b in col_bits.tolist())
    return hex(int(s or '0', 2))

def baseline_aligned_cell(font: ImageFont.FreeTypeFont, ch: str):
    """Return (bitmap_bits, cell_width, line_height) for ch using baseline alignment."""
    ascent, descent = font.getmetrics()
    line_height = ascent + descent
    # Prefer typographic advance; fall back to getsize width if Pillow is older
    try:
        advance = int(ceil(font.getlength(ch)))
    except Exception:
        advance = max(1, font.getsize(ch)[0])

    W = max(1, advance)
    H = max(1, line_height)

    # 1-bit image (white background = 1), draw text in black = 0
    img = Image.new('1', (W, H), 1)
    draw = ImageDraw.Draw(img)
    # anchor "ls": left, baseline. Baseline y = ascent.
    draw.text((0, ascent), ch, font=font, fill=0, anchor="ls")

    arr = np.array(img)
    bits = (arr == 0).astype(np.uint8)  # 1 where ink present
    return bits, W, H

def emit_setglyph_line(family_name: str, u: int, bits: np.ndarray, width: int, height: int) -> str:
    hex_cols = [col_to_hex(bits[:, x]) for x in range(bits.shape[1])]
    hexs_str = ", ".join(hex_cols)
    return (
        f"\t{family_name}.setGlyph({hex(u)}, "
        f"{{{{{hexs_str}}}, {width}, {height}, 0, 0}}); "
        f"// Glyph {hex(u)} - '{chr(u)}'"
    )

def default_codepoints() -> Iterable[int]:
    # Printable ASCII (space excluded because many monospace fonts treat it as zero-ink)
    return range(0x21, 0x7F)

def sanitize_family(font_path: str) -> str:
    base = Path(font_path).stem
    # Make it a valid-ish identifier for the target code
    return ''.join(c if c.isalnum() else '_' for c in base)

def main():
    ap = argparse.ArgumentParser(description="Export baseline-aligned 1bpp glyphs as column-hex")
    ap.add_argument("--font", required=True, help="Path to .ttf/.otf (e.g., FiraCode-Light.ttf)")
    ap.add_argument("--size", type=int, default=12, help="Pixel size (default: 12)")
    ap.add_argument("--range", default="0x21-0x7E", help="Codepoint range like 0x21-0x7E or CSV (e.g., 0x20,0x41,0x7E)")
    ap.add_argument("--family", default=None, help="Family name to print (default: derived from font file name)")
    ap.add_argument("--out", default=None, help="Output file (default: <font>_glyphs_<size>px.txt)")
    args = ap.parse_args()

    font = load_font(args.font, args.size)
    family = args.family or sanitize_family(args.font)

    # Parse codepoint selection
    cps: Iterable[int]
    if "-" in args.range and args.range.count("-") == 1 and "," not in args.range:
        lo_str, hi_str = args.range.split("-")
        cps = range(int(lo_str, 0), int(hi_str, 0) + 1)
    else:
        cps = [int(tok, 0) for tok in args.range.split(",") if tok.strip()]

    lines = []
    for u in cps:
        bits, w, h = baseline_aligned_cell(font, chr(u))
        lines.append(emit_setglyph_line(family, u, bits, w, h))

    out_path = (
        Path(args.out)
        if args.out
        else Path(f"{sanitize_family(args.font)}_glyphs_{args.size}px.txt")
    )
    out_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {out_path} ({len(lines)} glyphs)")

if __name__ == "__main__":
    main()
