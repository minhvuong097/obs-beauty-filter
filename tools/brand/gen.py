#!/usr/bin/env python3
"""Generate the OBS Beauty Filter brand kit (SVG + PNG).

Usage:  python3 tools/brand/gen.py
Requires fontTools, and rsvg-convert for the PNG exports.
The Manrope font files in tools/brand/fonts are instanced from the variable
font (SIL Open Font License 1.1, see fonts/OFL.txt).
"""

import os
import shutil
import subprocess
import sys

from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT_SVG = os.path.join(ROOT, "assets", "brand", "svg")
OUT_PNG = os.path.join(ROOT, "assets", "brand", "png")
os.makedirs(OUT_SVG, exist_ok=True)
os.makedirs(OUT_PNG, exist_ok=True)

ROSE = "#FF5C9E"
PEACH = "#FFB36B"
VIOLET = "#9B7BFF"
INK = "#0B1020"
INK_2 = "#1A2340"
TEXT = "#F7F8FC"
MUTED = "#93A5C7"
LIGHT_TEXT = "#0D1424"
LIGHT_MUTED = "#5B6B8C"

# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------


def polar(cx, cy, r, deg):
    import math

    a = math.radians(deg)
    return cx + r * math.cos(a), cy + r * math.sin(a)


def sparkle(cx, cy, s, fill, opacity=1.0):
    q = s * 0.085
    d = (
        f"M {cx:.2f} {cy - s:.2f} "
        f"Q {cx + q:.2f} {cy - q:.2f} {cx + s:.2f} {cy:.2f} "
        f"Q {cx + q:.2f} {cy + q:.2f} {cx:.2f} {cy + s:.2f} "
        f"Q {cx - q:.2f} {cy + q:.2f} {cx - s:.2f} {cy:.2f} "
        f"Q {cx - q:.2f} {cy - q:.2f} {cx:.2f} {cy - s:.2f} Z"
    )
    return f'<path d="{d}" fill="{fill}" opacity="{opacity}"/>'


def blade_path(a0, a1, R, r, gap):
    x0, y0 = polar(0, 0, R, a0 + gap)
    x1, y1 = polar(0, 0, R, a1 - gap)
    ix1, iy1 = polar(0, 0, r, a1)
    ix0, iy0 = polar(0, 0, r, a0)
    return (
        f"M {x0:.2f} {y0:.2f} A {R} {R} 0 0 1 {x1:.2f} {y1:.2f} "
        f"L {ix1:.2f} {iy1:.2f} L {ix0:.2f} {iy0:.2f} Z"
    )


MARK_R = 166.0
MARK_r = 74.0
MARK_GAP = 13.0


def marker(cx=256, cy=252, scale=1.0, mono=None):
    """Aperture-iris mark: six blades, glowing iris core, beauty sparkles."""
    g = [f'<g transform="translate({cx} {cy}) scale({scale})">']

    if mono:
        petal = mono
        core_fill = mono
        spark = mono
    else:
        petal = "url(#petalGrad)"
        core_fill = "url(#core)"
        spark = "#FFFFFF"

    if not mono:
        g.append('<circle r="150" fill="url(#markGlow)"/>')

    for k in range(6):
        a0 = -60 + k * 60
        a1 = a0 + 60
        d = blade_path(a0, a1, MARK_R, MARK_r, MARK_GAP)
        g.append(
            f'<path d="{d}" fill="{petal}" stroke="{petal}" '
            f'stroke-width="4" stroke-linejoin="round"/>'
        )
        if not mono:
            rx0, ry0 = polar(0, 0, MARK_R + 1.5, a0 + MARK_GAP + 1.5)
            rx1, ry1 = polar(0, 0, MARK_R + 1.5, a1 - MARK_GAP - 1.5)
            g.append(
                f'<path d="M {rx0:.2f} {ry0:.2f} A {MARK_R + 1.5} {MARK_R + 1.5} 0 0 1 '
                f'{rx1:.2f} {ry1:.2f}" fill="none" stroke="#FFFFFF" '
                f'stroke-opacity="0.22" stroke-width="3.5" stroke-linecap="round"/>'
            )

    if mono:
        g.append(f'<circle r="27" fill="none" stroke="{core_fill}" stroke-width="7"/>')
        g.append(sparkle(0, 0, 33, spark, 1.0))
    else:
        g.append('<circle r="26" fill="#0B1020"/>')
        g.append(f'<circle r="26" fill="{core_fill}"/>')
        g.append(sparkle(0, 0, 42, spark, 0.97))

    g.append(sparkle(150, -104, 13, spark, 0.9))
    g.append("</g>")
    return "".join(g)


def icon(size=512, x=0, y=0, mono=None, rounded=True):
    """App-icon tile with the mark."""
    k = size / 512.0
    g = [f'<g transform="translate({x} {y}) scale({k})">']
    if rounded and not mono:
        fill = "url(#tile)"
        g.append(
            f'<rect x="6" y="6" width="500" height="500" rx="118" '
            f'fill="{fill}"/>'
        )
        if True:
            g.append(
                '<rect x="6" y="6" width="500" height="500" rx="118" '
                'fill="none" stroke="#FFFFFF" stroke-opacity="0.06" '
                'stroke-width="3"/>'
            )
            g.append('<circle cx="256" cy="250" r="240" fill="url(#tileGlow)"/>')
    g.append(marker(mono=mono))
    g.append("</g>")
    return "".join(g)


DEFS = f"""<defs>
  <linearGradient id="tile" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="#11182C"/>
    <stop offset="1" stop-color="#1C2542"/>
  </linearGradient>
  <radialGradient id="tileGlow" cx="0.30" cy="0.22" r="0.85">
    <stop offset="0" stop-color="{ROSE}" stop-opacity="0.20"/>
    <stop offset="0.55" stop-color="{ROSE}" stop-opacity="0.05"/>
    <stop offset="1" stop-color="{ROSE}" stop-opacity="0"/>
  </radialGradient>
  <linearGradient id="accent" x1="0.10" y1="0.05" x2="0.90" y2="1.00">
    <stop offset="0" stop-color="{ROSE}"/>
    <stop offset="0.55" stop-color="#FF8A8A"/>
    <stop offset="1" stop-color="{PEACH}"/>
  </linearGradient>
  <linearGradient id="petalGrad" gradientUnits="userSpaceOnUse"
                  x1="-170" y1="-180" x2="170" y2="180">
    <stop offset="0" stop-color="#FF6FA8"/>
    <stop offset="0.50" stop-color="{ROSE}"/>
    <stop offset="1" stop-color="{PEACH}"/>
  </linearGradient>
  <radialGradient id="markGlow" cx="0.5" cy="0.5" r="0.5">
    <stop offset="0" stop-color="{PEACH}" stop-opacity="0.42"/>
    <stop offset="0.45" stop-color="{ROSE}" stop-opacity="0.20"/>
    <stop offset="1" stop-color="{ROSE}" stop-opacity="0"/>
  </radialGradient>
  <radialGradient id="core" cx="0.38" cy="0.32" r="0.75">
    <stop offset="0" stop-color="#FFFFFF"/>
    <stop offset="0.45" stop-color="#FFE2C4"/>
    <stop offset="1" stop-color="{PEACH}"/>
  </radialGradient>
</defs>"""


# ---------------------------------------------------------------------------
# Fonts
# ---------------------------------------------------------------------------


class Wordmark:
    def __init__(self, path):
        self.tt = TTFont(path)
        self.upem = self.tt["head"].unitsPerEm
        self.cmap = self.tt.getBestCmap()
        self.gs = self.tt.getGlyphSet()
        self.hmtx = self.tt["hmtx"]

    def draw(self, text, size, x, y, fill, tracking_em=0.0):
        s = size / self.upem
        penx = 0.0
        out = [
            f'<g transform="translate({x:.3f} {y:.3f}) scale({s:.6f} -{s:.6f})" '
            f'fill="{fill}">'
        ]
        for ch in text:
            gname = self.cmap.get(ord(ch))
            if gname is None:
                continue
            pen = SVGPathPen(self.gs)
            self.gs[gname].draw(pen)
            d = pen.getCommands()
            if d:
                out.append(f'<path transform="translate({penx:.2f} 0)" d="{d}"/>')
            penx += self.hmtx[gname][0] + tracking_em * self.upem
        out.append("</g>")
        width = penx * s
        if out:
            width -= tracking_em * size  # drop trailing tracking from centring
        return "".join(out), width


FONT_DIR = os.path.join(HERE, "fonts")
semibold = Wordmark(os.path.join(FONT_DIR, "Manrope-SemiBold.ttf"))
bold = Wordmark(os.path.join(FONT_DIR, "Manrope-Bold.ttf"))
extrabold = Wordmark(os.path.join(FONT_DIR, "Manrope-ExtraBold.ttf"))


# ---------------------------------------------------------------------------
# Lockups
# ---------------------------------------------------------------------------


def horizontal(text_color, muted_color, tile=True, mono=None):
    pad_x, pad_y = 8, 8
    icon_size = 132
    gap = 34
    name_size = 60
    obs_size = 21

    tx = pad_x + (icon_size if tile else 0) + (gap if tile else 0)
    beauty_y = pad_y + 118
    obs_y = pad_y + 52

    body, beauty_w = extrabold.draw("Beauty Filter", name_size, tx, beauty_y, text_color,
                                    tracking_em=-0.015)
    label, obs_w = bold.draw("OBS", obs_size, tx + 3, obs_y, muted_color,
                             tracking_em=0.32)

    width = int(tx + max(beauty_w, obs_w) + pad_x + 14)
    height = icon_size + 2 * pad_y

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}" role="img" '
        f'aria-label="OBS Beauty Filter">',
        DEFS,
    ]
    if tile:
        parts.append(icon(icon_size, pad_x, pad_y, mono=mono))
    else:
        parts.append(marker(icon_size / 2 + pad_x, icon_size / 2 + pad_y,
                            icon_size / 512, mono=mono))
    parts.append(label)
    parts.append(body)
    parts.append("</svg>")
    return "\n".join(parts)


def stacked(text_color, muted_color, mono=None):
    size = 600
    icon_size = 200
    icon_x = (size - icon_size) / 2
    name_size = 62
    obs_size = 21

    _, beauty_w = extrabold.draw("Beauty Filter", name_size, 0, 0, text_color)
    _, obs_w = bold.draw("OBS", obs_size, 0, 0, muted_color, tracking_em=0.32)

    beauty_x = (size - beauty_w) / 2
    obs_x = (size - obs_w) / 2
    height = 410

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{height}" '
        f'viewBox="0 0 {size} {height}" role="img" '
        f'aria-label="OBS Beauty Filter">',
        DEFS,
        icon(icon_size, icon_x, 8, mono=mono),
        bold.draw("OBS", obs_size, obs_x, 268, muted_color, tracking_em=0.32)[0],
        extrabold.draw("Beauty Filter", name_size, beauty_x, 342, text_color,
                       tracking_em=-0.015)[0],
        "</svg>",
    ]
    return "\n".join(parts)


def banner():
    w, h = 1280, 640
    icon_size = 184
    gap = 52
    name_size = 64
    obs_size = 22
    sub_size = 24

    _, name_w = extrabold.draw("Beauty Filter", name_size, 0, 0, TEXT,
                               tracking_em=-0.015)
    _, obs_w = bold.draw("OBS", obs_size, 0, 0, MUTED, tracking_em=0.32)
    _, sub_w = semibold.draw("Real-time GPU beauty for OBS Studio", sub_size, 0, 0,
                             MUTED, tracking_em=0.01)

    text_w = max(name_w, obs_w, sub_w)
    group_w = icon_size + gap + text_w
    icon_x = (w - group_w) / 2
    tx = icon_x + icon_size + gap
    icon_y = (h - icon_size) / 2

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
        f'viewBox="0 0 {w} {h}" role="img" aria-label="OBS Beauty Filter">',
        DEFS,
        f'<rect width="{w}" height="{h}" fill="url(#tile)"/>',
        '<circle cx="120" cy="10" r="620" fill="url(#tileGlow)" opacity="0.75"/>',
        '<circle cx="1240" cy="660" r="520" fill="url(#markGlow)" opacity="0.45"/>',
        icon(icon_size, icon_x, icon_y),
        bold.draw("OBS", obs_size, tx + 2, 268, MUTED, tracking_em=0.32)[0],
        extrabold.draw("Beauty Filter", name_size, tx, 344, TEXT,
                       tracking_em=-0.015)[0],
        f'<rect x="{tx + 2:.1f}" y="372" width="72" height="5" rx="2.5" '
        f'fill="url(#accent)"/>',
        semibold.draw("Real-time GPU beauty for OBS Studio", sub_size, tx,
                      420, MUTED, tracking_em=0.01)[0],
        "</svg>",
    ]
    return "\n".join(parts)


# ---------------------------------------------------------------------------
# Emit
# ---------------------------------------------------------------------------

files = {
    "icon.svg": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" '
        'viewBox="0 0 512 512" role="img" aria-label="OBS Beauty Filter">'
        + DEFS + icon(512, 0, 0) + "</svg>"
    ),
    "mark.svg": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" '
        'viewBox="0 0 512 512" role="img" aria-label="OBS Beauty Filter">'
        + DEFS + marker() + "</svg>"
    ),
    "logo-horizontal-dark.svg": horizontal(TEXT, MUTED),
    "logo-horizontal-light.svg": horizontal(LIGHT_TEXT, LIGHT_MUTED),
    "logo-horizontal-mono-white.svg": horizontal("#FFFFFF", "#FFFFFF", mono="#FFFFFF"),
    "logo-horizontal-mono-black.svg": horizontal("#0B1020", "#0B1020", mono="#0B1020"),
    "logo-stacked-dark.svg": stacked(TEXT, MUTED),
    "logo-stacked-light.svg": stacked(LIGHT_TEXT, LIGHT_MUTED),
    "logo-stacked-mono-white.svg": stacked("#FFFFFF", "#FFFFFF", mono="#FFFFFF"),
    "logo-stacked-mono-black.svg": stacked("#0B1020", "#0B1020", mono="#0B1020"),
    "banner.svg": banner(),
}

for name, content in files.items():
    with open(os.path.join(OUT_SVG, name), "w", encoding="utf-8") as fh:
        fh.write(content)
    print("wrote", os.path.join("assets", "brand", "svg", name))

# PNG exports.  Install librsvg (rsvg-convert) or skip this step; the SVGs are
# the source of truth and PNGs are convenience copies for README/social use.
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

rsvg = shutil.which("rsvg-convert")
if not rsvg:
    print("rsvg-convert not found: skipped PNG export", file=sys.stderr)
else:
    for src_name, out_name, width in PNG_EXPORTS:
        subprocess.run(
            [rsvg, "-w", str(width), os.path.join(OUT_SVG, src_name),
             "-o", os.path.join(OUT_PNG, out_name)],
            check=True,
        )
        print("wrote", os.path.join("assets", "brand", "png", out_name))


def _decode_png(data):
    """Minimal PNG reader for 8-bit RGBA, non-interlaced images (stdlib only).

    Returns (width, height, bytearray of RGBA rows).
    """
    import struct
    import zlib

    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos = 8
    idat = bytearray()
    width = height = depth = ctype = interlace = None
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
    """Encode one frame as a 32-bit BMP icon entry (Inno Setup rejects PNG)."""
    import struct

    header = struct.pack(
        "<IiiHHIIiiII",
        40, width, height * 2, 1, 32, 0, width * height * 4, 0, 0, 0, 0,
    )
    xor = bytearray()
    for y in range(height - 1, -1, -1):
        row = rgba[y * width * 4:(y + 1) * width * 4]
        for x in range(width):
            r, g, b, a = row[x * 4:x * 4 + 4]
            xor += bytes((b, g, r, a))
    mask_stride = ((width + 31) // 32) * 4
    mask = bytes(mask_stride * height)
    return header + bytes(xor) + mask


def write_ico(path, png_paths):
    """Pack PNG renders into a multi-resolution .ico with BMP frames."""
    import struct

    frames = []
    for png in png_paths:
        with open(png, "rb") as fh:
            data = fh.read()
        width, height, rgba = _decode_png(data)
        frames.append((width, height, _ico_frame(width, height, rgba)))

    header = struct.pack("<HHH", 0, 1, len(frames))
    offset = 6 + 16 * len(frames)
    entries = b""
    for width, height, blob in frames:
        entries += struct.pack(
            "<BBBBHHII",
            0 if width >= 256 else width,
            0 if height >= 256 else height,
            0, 0, 1, 32, len(blob), offset,
        )
        offset += len(blob)

    with open(path, "wb") as fh:
        fh.write(header + entries + b"".join(f[2] for f in frames))
    print("wrote", os.path.relpath(path, ROOT))


if rsvg:
    write_ico(
        os.path.join(OUT_PNG, "icon.ico"),
        [os.path.join(OUT_PNG, f"icon-{size}.png") for size in (16, 32, 48, 64, 128, 256)],
    )
