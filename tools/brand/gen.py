#!/usr/bin/env python3
"""Generate the OBS Beauty Filter brand kit.

The mark is the six-petal aperture flower; the wordmark is the full product
name set on a single line next to it (Manrope, OFL).

    python3 tools/brand/gen.py

Needs fontTools. When ``rsvg-convert`` is on PATH the PNG/ICO exports are
regenerated too.
"""
import os
import shutil
import struct
import subprocess
import sys
import zlib

from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.ttLib import TTFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT_SVG = os.path.join(ROOT, "assets", "brand", "svg")
OUT_PNG = os.path.join(ROOT, "assets", "brand", "png")
FONT_DIR = os.path.join(HERE, "fonts")

# ── palette ────────────────────────────────────────────────────────────────
TILE_IN = "#1D1145"
TILE_OUT = "#0A0720"
PETALS = ["#FF7AB8", "#FF9A62", "#FFD36B", "#5EEAD4", "#6EA8FF", "#B14CF2"]
CORE = "#120C2A"
WHITE = "#F7F8FC"
MUTED = "#A9B3D6"
INK = "#0D1424"
MUTED_LIGHT = "#5B6B8C"

PETAL_D = "M100 100 C114 80 136 56 100 32 C64 56 86 80 100 100 Z"
SPARK_D = (
    "M100 93 C100.84 98.32 101.68 99.16 107 100 C101.68 100.84 "
    "100.84 101.68 100 107 C99.16 101.68 98.32 100.84 93 100 "
    "C98.32 99.16 99.16 98.32 100 93 Z"
)
# The source artwork is drawn on a 184×184 tile inside a 200×200 box.
TILE_BOX = 184.0
TILE_ORIGIN = -8.0


class Typeface:
    """A static font with SVG path extraction and real ink metrics."""

    def __init__(self, filename):
        self.tt = TTFont(os.path.join(FONT_DIR, filename))
        self.upem = self.tt["head"].unitsPerEm
        self.cap = self.tt["OS/2"].sCapHeight
        self.cmap = self.tt.getBestCmap()
        self.gs = self.tt.getGlyphSet()
        self.hmtx = self.tt["hmtx"]

    def _runs(self, text, tracking_em):
        penx = 0.0
        for ch in text:
            name = self.cmap.get(ord(ch))
            if name is not None:
                yield name, penx
            penx += self.hmtx[name][0] + tracking_em * self.upem

    def advance(self, text, size, tracking_em=0.0):
        units = 0.0
        for ch in text:
            name = self.cmap.get(ord(ch))
            if name:
                units += self.hmtx[name][0] + tracking_em * self.upem
        if text:
            units -= tracking_em * self.upem
        return units * size / self.upem

    def ink(self, text, size, tracking_em=0.0):
        """Ink box in px relative to the pen origin: (x0, y0, x1, y1)."""
        s = size / self.upem
        x0 = y0 = 1e9
        x1 = y1 = -1e9
        for name, dx in self._runs(text, tracking_em):
            pen = BoundsPen(self.gs)
            self.gs[name].draw(pen)
            if not pen.bounds:
                continue
            gx0, gy0, gx1, gy1 = pen.bounds
            x0 = min(x0, (dx + gx0) * s)
            x1 = max(x1, (dx + gx1) * s)
            y0 = min(y0, gy0 * s)
            y1 = max(y1, gy1 * s)
        return x0, y0, x1, y1

    def draw(self, text, size, x, y, fill, tracking_em=0.0, opacity=None):
        s = size / self.upem
        alpha = f' fill-opacity="{opacity}"' if opacity is not None else ""
        out = [
            f'<g transform="translate({x:.3f} {y:.3f}) scale({s:.6f} -{s:.6f})" '
            f'fill="{fill}"{alpha}>'
        ]
        for name, dx in self._runs(text, tracking_em):
            pen = SVGPathPen(self.gs)
            self.gs[name].draw(pen)
            d = pen.getCommands()
            if d:
                out.append(f'<path transform="translate({dx:.2f} 0)" d="{d}"/>')
        out.append("</g>")
        return "".join(out)


extra_bold = Typeface("Manrope-ExtraBold.ttf")
bold = Typeface("Manrope-Bold.ttf")
semi_bold = Typeface("Manrope-SemiBold.ttf")

NAME = "Beauty Filter"
NAME_TRACK = -0.02
PREFIX = "OBS "
PREFIX_TRACK = 0.01


DEFS = f"""<defs>
  <linearGradient id="tile" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="{TILE_IN}"/>
    <stop offset="1" stop-color="{TILE_OUT}"/>
  </linearGradient>
  <linearGradient id="gloss" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#FFFFFF" stop-opacity="0.30"/>
    <stop offset="1" stop-color="#FFFFFF" stop-opacity="0"/>
  </linearGradient>
  <clipPath id="tileClip">
    <rect x="8" y="8" width="184" height="184" rx="46"/>
  </clipPath>
</defs>"""


def icon(size, x=0, y=0, mono=None, contrast="#FFFFFF"):
    """The aperture flower on its tile. ``mono`` renders a one-colour mark."""
    k = size / TILE_BOX
    parts = [
        f'<g transform="translate({x} {y}) scale({k:.6f}) translate({TILE_ORIGIN} {TILE_ORIGIN})">'
    ]
    if mono:
        # one-colour mark: solid flower with the iris knocked out
        parts.append(
            "".join(
                f'<path d="{PETAL_D}" fill="{mono}" transform="rotate({i * 60} 100 100)"/>'
                for i in range(6)
            )
        )
        parts.append(f'<circle cx="100" cy="100" r="11" fill="{contrast}"/>')
        parts.append(f'<path d="{SPARK_D}" fill="{mono}"/>')
    else:
        parts.append('<rect x="8" y="8" width="184" height="184" rx="46" fill="url(#tile)"/>')
        parts.append(
            '<g clip-path="url(#tileClip)"><rect x="8" y="8" width="184" height="86" '
            'fill="url(#gloss)"/></g>'
        )
        parts.append(
            '<rect x="8.75" y="8.75" width="182.5" height="182.5" rx="45.5" fill="none" '
            'stroke="#FFFFFF" stroke-opacity="0.12" stroke-width="1.5"/>'
        )
        for i, colour in enumerate(PETALS):
            parts.append(
                f'<path d="{PETAL_D}" fill="{colour}" fill-opacity="0.92" '
                f'transform="rotate({i * 60} 100 100)"/>'
            )
        parts.append(f'<circle cx="100" cy="100" r="11" fill="{CORE}"/>')
        parts.append(f'<path d="{SPARK_D}" fill="{contrast}"/>')
    parts.append("</g>")
    return "".join(parts)


def flower(size, x=0, y=0, mono=None, contrast="#FFFFFF"):
    """The flower alone, no tile, filling a square of ``size``."""
    k = size / 150.0
    parts = [f'<g transform="translate({x + size / 2:.3f} {y + size / 2:.3f}) '
             f'scale({k:.6f}) translate(-100 -100)">']
    for i, colour in enumerate(PETALS):
        fill = mono or colour
        opacity = "" if mono else ' fill-opacity="0.92"'
        parts.append(
            f'<path d="{PETAL_D}" fill="{fill}"{opacity} transform="rotate({i * 60} 100 100)"/>'
        )
    parts.append(f'<circle cx="100" cy="100" r="11" fill="{mono and contrast or CORE}"/>')
    parts.append(f'<path d="{SPARK_D}" fill="{mono or "#FFFFFF"}"/>')
    parts.append("</g>")
    return "".join(parts)


def wordmark(size, x, baseline, name_color, prefix_color, prefix_opacity=None):
    """`OBS Beauty Filter` on one baseline; returns (svg, ink width)."""
    nx0, _, nx1, _ = extra_bold.ink(NAME, size, NAME_TRACK)
    prefix_w = bold.advance(PREFIX, size, PREFIX_TRACK)
    body = bold.draw(PREFIX, size, x, baseline, prefix_color, PREFIX_TRACK, prefix_opacity)
    body += extra_bold.draw(NAME, size, x + prefix_w - nx0, baseline, name_color, NAME_TRACK)
    return body, prefix_w + (nx1 - nx0)


def horizontal(name_color, prefix_color, mono=None, icon_size=116, gap=26, pad=10,
               name_size=58):
    height = icon_size + 2 * pad
    cap = extra_bold.cap * name_size / extra_bold.upem
    baseline = pad + icon_size / 2 + cap / 2
    tx = pad + icon_size + gap
    body, text_w = wordmark(name_size, tx, baseline, name_color, prefix_color,
                            0.72 if mono else None)
    width = int(round(tx + text_w + pad + 8))
    contrast = white_or_ink(mono)
    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}" role="img" aria-label="OBS Beauty Filter">',
        DEFS,
        icon(icon_size, pad, pad, mono=mono, contrast=contrast),
        body,
        "</svg>",
    ]
    return "\n".join(svg)


def white_or_ink(mono):
    """Sparkle colour that contrasts with a one-colour mark."""
    if mono in ("#FFFFFF", "#F7F8FC"):
        return TILE_OUT
    return "#FFFFFF"


def stacked(name_color, prefix_color, mono=None):
    width, icon_size, name_size = 640, 168, 46
    cap = extra_bold.cap * name_size / extra_bold.upem
    nx0, _, nx1, _ = extra_bold.ink(NAME, name_size, NAME_TRACK)
    prefix_w = bold.advance(PREFIX, name_size, PREFIX_TRACK)
    text_w = prefix_w + (nx1 - nx0)

    icon_y = 4
    baseline = icon_y + icon_size + 34 + cap
    x = (width - text_w) / 2 - nx0
    body, _ = wordmark(name_size, x, baseline, name_color, prefix_color,
                       0.72 if mono else None)
    height = int(round(baseline + 16))
    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}" role="img" aria-label="OBS Beauty Filter">',
        DEFS,
        icon(icon_size, (width - icon_size) / 2, icon_y, mono=mono,
             contrast=white_or_ink(mono)),
        body,
        "</svg>",
    ]
    return "\n".join(svg)


def banner():
    width, height = 1280, 640
    icon_size = 212
    gap = 60
    name_size = 74
    tagline_size = 27

    cap = extra_bold.cap * name_size / extra_bold.upem
    nx0, _, nx1, _ = extra_bold.ink(NAME, name_size, NAME_TRACK)
    prefix_w = bold.advance(PREFIX, name_size, PREFIX_TRACK)
    text_w = prefix_w + (nx1 - nx0)
    tagline = "Real-time GPU beauty for OBS Studio"
    tagline_w = semi_bold.advance(tagline, tagline_size, 0.01)

    group_w = icon_size + gap + max(text_w, tagline_w)
    icon_x = (width - group_w) / 2
    tx = icon_x + icon_size + gap
    # centre the name + rule + tagline block on the canvas
    baseline = (height + cap - 88) / 2
    body, _ = wordmark(name_size, tx - nx0, baseline, WHITE, MUTED)

    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}" role="img" aria-label="OBS Beauty Filter">',
        DEFS,
        '<defs>'
        '<radialGradient id="glowA" cx="4%" cy="-8%" r="80%">'
        f'<stop offset="0" stop-color="#B14CF2" stop-opacity="0.34"/>'
        '<stop offset="0.55" stop-color="#B14CF2" stop-opacity="0.08"/>'
        '<stop offset="1" stop-color="#B14CF2" stop-opacity="0"/>'
        "</radialGradient>"
        '<radialGradient id="glowB" cx="108%" cy="118%" r="70%">'
        f'<stop offset="0" stop-color="#FF9A62" stop-opacity="0.26"/>'
        '<stop offset="0.6" stop-color="#FF9A62" stop-opacity="0.06"/>'
        '<stop offset="1" stop-color="#FF9A62" stop-opacity="0"/>'
        "</radialGradient>"
        "</defs>",
        f'<rect width="{width}" height="{height}" fill="url(#tile)"/>',
        f'<rect width="{width}" height="{height}" fill="url(#glowA)"/>',
        f'<rect width="{width}" height="{height}" fill="url(#glowB)"/>',
        icon(icon_size, icon_x, (height - icon_size) / 2),
        body,
        f'<rect x="{tx - nx0:.1f}" y="{baseline + 34:.1f}" width="84" height="4" rx="2" '
        'fill="#FF7AB8"/>',
        semi_bold.draw(tagline, tagline_size, tx - nx0, baseline + 92, MUTED, 0.01),
        "</svg>",
    ]
    return "\n".join(svg)


FILES = {
    "icon.svg": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" '
        'viewBox="0 0 512 512" role="img" aria-label="OBS Beauty Filter">'
        + DEFS + icon(512, 0, 0) + "</svg>"
    ),
    "mark.svg": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" '
        'viewBox="0 0 512 512" role="img" aria-label="OBS Beauty Filter">'
        + DEFS + flower(512) + "</svg>"
    ),
    "logo-horizontal-dark.svg": horizontal(WHITE, MUTED),
    "logo-horizontal-light.svg": horizontal(INK, MUTED_LIGHT),
    "logo-horizontal-mono-white.svg": horizontal("#FFFFFF", "#FFFFFF", mono="#FFFFFF"),
    "logo-horizontal-mono-black.svg": horizontal("#0B1020", "#0B1020", mono="#0B1020"),
    "logo-stacked-dark.svg": stacked(WHITE, MUTED),
    "logo-stacked-light.svg": stacked(INK, MUTED_LIGHT),
    "logo-stacked-mono-white.svg": stacked("#FFFFFF", "#FFFFFF", mono="#FFFFFF"),
    "logo-stacked-mono-black.svg": stacked("#0B1020", "#0B1020", mono="#0B1020"),
    "banner.svg": banner(),
}

PNG_EXPORTS = [
    ("icon.svg", "icon-512.png", 512),
    ("icon.svg", "icon-256.png", 256),
    ("icon.svg", "icon-128.png", 128),
    ("icon.svg", "icon-64.png", 64),
    ("icon.svg", "icon-48.png", 48),
    ("icon.svg", "icon-32.png", 32),
    ("icon.svg", "icon-16.png", 16),
    ("logo-horizontal-dark.svg", "logo-horizontal-dark.png", 1024),
    ("logo-horizontal-light.svg", "logo-horizontal-light.png", 1024),
    ("logo-horizontal-mono-white.svg", "logo-horizontal-mono-white.png", 1024),
    ("logo-horizontal-mono-black.svg", "logo-horizontal-mono-black.png", 1024),
    ("logo-stacked-dark.svg", "logo-stacked-dark.png", 640),
    ("logo-stacked-light.svg", "logo-stacked-light.png", 640),
    ("banner.svg", "banner.png", 1280),
    ("banner.svg", "social-preview.png", 1280),
]


def _decode_png(data):
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos, idat = 8, bytearray()
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    if depth != 8 or ctype != 6 or interlace != 0:
        raise ValueError("expected 8-bit RGBA non-interlaced PNG")

    raw = zlib.decompress(bytes(idat))
    stride = width * 4
    out = bytearray(height * stride)
    prev = bytearray(stride)
    off = 0
    for y in range(height):
        ftype = raw[off]
        off += 1
        line = bytearray(raw[off:off + stride])
        off += stride
        if ftype == 1:
            for i in range(4, stride):
                line[i] = (line[i] + line[i - 4]) & 0xFF
        elif ftype == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:
            for i in range(stride):
                left = line[i - 4] if i >= 4 else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:
            for i in range(stride):
                a = line[i - 4] if i >= 4 else 0
                b = prev[i]
                c = prev[i - 4] if i >= 4 else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
        elif ftype != 0:
            raise ValueError(f"bad PNG filter {ftype}")
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return width, height, out


def _ico_frame(width, height, rgba):
    header = struct.pack(
        "<IiiHHIIiiII", 40, width, height * 2, 1, 32, 0, width * height * 4, 0, 0, 0, 0
    )
    xor = bytearray()
    for y in range(height - 1, -1, -1):
        row = rgba[y * width * 4:(y + 1) * width * 4]
        for x in range(width):
            r, g, b, a = row[x * 4:x * 4 + 4]
            xor += bytes((b, g, r, a))
    mask_stride = ((width + 31) // 32) * 4
    return header + bytes(xor) + bytes(mask_stride * height)


def write_ico(path, png_paths):
    """Write a BMP-frame ICO; Inno Setup rejects PNG-compressed frames."""
    frames = []
    for png in png_paths:
        with open(png, "rb") as fh:
            width, height, rgba = _decode_png(fh.read())
        frames.append((width, height, _ico_frame(width, height, rgba)))

    entries = b""
    offset = 6 + 16 * len(frames)
    for width, height, blob in frames:
        entries += struct.pack(
            "<BBBBHHII",
            0 if width >= 256 else width,
            0 if height >= 256 else height,
            0, 0, 1, 32, len(blob), offset,
        )
        offset += len(blob)
    with open(path, "wb") as fh:
        fh.write(struct.pack("<HHH", 0, 1, len(frames)) + entries
                 + b"".join(f[2] for f in frames))
    print("wrote", os.path.relpath(path, ROOT))


def main():
    os.makedirs(OUT_SVG, exist_ok=True)
    os.makedirs(OUT_PNG, exist_ok=True)
    for name, content in FILES.items():
        with open(os.path.join(OUT_SVG, name), "w", encoding="utf-8") as fh:
            fh.write(content)
        print("wrote", os.path.join("assets", "brand", "svg", name))

    rsvg = shutil.which("rsvg-convert")
    if not rsvg:
        print("rsvg-convert not found: skipped PNG export", file=sys.stderr)
        return
    for src_name, out_name, width in PNG_EXPORTS:
        subprocess.run(
            [rsvg, "-w", str(width), os.path.join(OUT_SVG, src_name),
             "-o", os.path.join(OUT_PNG, out_name)],
            check=True,
        )
        print("wrote", os.path.join("assets", "brand", "png", out_name))
    write_ico(
        os.path.join(OUT_PNG, "icon.ico"),
        [os.path.join(OUT_PNG, f"icon-{s}.png") for s in (16, 32, 48, 64, 128, 256)],
    )


if __name__ == "__main__":
    main()
