#!/usr/bin/env python3
"""Generate the OBS Beauty Filter brand kit: serif wordmark + B monogram."""
import os
import shutil
import struct
import subprocess
import sys
import zlib

from fontTools.pens.boundsPen import BoundsPen
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT_SVG = os.path.join(ROOT, "assets", "brand", "svg")
OUT_PNG = os.path.join(ROOT, "assets", "brand", "png")
FONT_DIR = os.path.join(HERE, "fonts")

ROSE = "#FF5C9E"
ROSE_LIGHT = "#FF7FA8"
PEACH = "#FFB36B"
TEXT = "#F7F8FC"
MUTED = "#93A5C7"
LIGHT_TEXT = "#0D1424"
LIGHT_MUTED = "#5B6B8C"


class WM:
    def __init__(self, filename):
        self.tt = TTFont(os.path.join(FONT_DIR, filename))
        self.upem = self.tt["head"].unitsPerEm
        self.cmap = self.tt.getBestCmap()
        self.gs = self.tt.getGlyphSet()
        self.hmtx = self.tt["hmtx"]

    def width_units(self, text, tracking_em=0.0):
        penx = 0.0
        for ch in text:
            g = self.cmap.get(ord(ch))
            if g:
                penx += self.hmtx[g][0] + tracking_em * self.upem
        return penx - (tracking_em * self.upem if text else 0)

    def body(self, text, size, x, y, fill, tracking_em=0.0):
        s = size / self.upem
        penx = 0.0
        out = [
            f'<g transform="translate({x:.3f} {y:.3f}) scale({s:.6f} -{s:.6f})" '
            f'fill="{fill}">'
        ]
        for ch in text:
            g = self.cmap.get(ord(ch))
            if g is None:
                continue
            pen = SVGPathPen(self.gs)
            self.gs[g].draw(pen)
            d = pen.getCommands()
            if d:
                out.append(f'<path transform="translate({penx:.2f} 0)" d="{d}"/>')
            penx += self.hmtx[g][0] + tracking_em * self.upem
        out.append("</g>")
        return "".join(out), penx * s

    def grad(self, gid, width_units):
        return (
            f'<linearGradient id="{gid}" gradientUnits="userSpaceOnUse" '
            f'x1="0" y1="0" x2="{width_units:.1f}" y2="0">'
            f'<stop offset="0" stop-color="{ROSE}"/>'
            f'<stop offset="0.55" stop-color="{ROSE_LIGHT}"/>'
            f'<stop offset="1" stop-color="{PEACH}"/>'
            f"</linearGradient>"
        )


fraunces = WM("Fraunces-600.ttf")
fraunces_b = WM("Fraunces-700.ttf")
manrope_b = WM("Manrope-Bold.ttf")
manrope_sb = WM("Manrope-SemiBold.ttf")

WORD = "Beauty Filter"
WORD_TRACK = -0.012
WORD_UNITS = fraunces.width_units(WORD, WORD_TRACK)
BEAUTY_UNITS = fraunces.width_units("Beauty", WORD_TRACK)

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


DEFS = f"""<defs>
  <linearGradient id="tile" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="#121A31"/>
    <stop offset="1" stop-color="#1D2745"/>
  </linearGradient>
  <radialGradient id="tileGlow" cx="0.28" cy="0.20" r="0.9">
    <stop offset="0" stop-color="{ROSE}" stop-opacity="0.22"/>
    <stop offset="0.55" stop-color="{ROSE}" stop-opacity="0.06"/>
    <stop offset="1" stop-color="{ROSE}" stop-opacity="0"/>
  </radialGradient>
  <linearGradient id="bgrad" gradientUnits="userSpaceOnUse"
                  x1="120" y1="1450" x2="1450" y2="-120">
    <stop offset="0" stop-color="{ROSE}"/>
    <stop offset="0.55" stop-color="{ROSE_LIGHT}"/>
    <stop offset="1" stop-color="{PEACH}"/>
  </linearGradient>
  <linearGradient id="wgrad" gradientUnits="userSpaceOnUse"
                  x1="0" y1="0" x2="{BEAUTY_UNITS:.1f}" y2="0">
    <stop offset="0" stop-color="{ROSE}"/>
    <stop offset="0.55" stop-color="{ROSE_LIGHT}"/>
    <stop offset="1" stop-color="{PEACH}"/>
  </linearGradient>
  <radialGradient id="bgGlowA" cx="8%" cy="0%" r="78%">
    <stop offset="0" stop-color="{ROSE}" stop-opacity="0.26"/>
    <stop offset="0.55" stop-color="{ROSE}" stop-opacity="0.07"/>
    <stop offset="1" stop-color="{ROSE}" stop-opacity="0"/>
  </radialGradient>
  <radialGradient id="bgGlowB" cx="106%" cy="112%" r="62%">
    <stop offset="0" stop-color="{PEACH}" stop-opacity="0.20"/>
    <stop offset="0.6" stop-color="{PEACH}" stop-opacity="0.05"/>
    <stop offset="1" stop-color="{PEACH}" stop-opacity="0"/>
  </radialGradient>
  <linearGradient id="wgradL" gradientUnits="userSpaceOnUse"
                  x1="0" y1="0" x2="{BEAUTY_UNITS:.1f}" y2="0">
    <stop offset="0" stop-color="#E23A74"/>
    <stop offset="0.55" stop-color="#F04A80"/>
    <stop offset="1" stop-color="#FF7A4D"/>
  </linearGradient>
  <linearGradient id="accent" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="{ROSE}"/>
    <stop offset="1" stop-color="{PEACH}"/>
  </linearGradient>
</defs>"""


def mark(cx=256, cy=252, scale=1.0, mono=None):
    """Serif B monogram inside a thin lens ring, with a beauty sparkle."""
    g = [f'<g transform="translate({cx} {cy}) scale({scale})">']

    if mono:
        b_fill = mono
        ring = mono
    else:
        b_fill = "url(#bgrad)"
        ring = "url(#bgrad)"

    size = 318.0
    s_ = size / fraunces_b.upem
    bp = BoundsPen(fraunces_b.gs)
    fraunces_b.gs[fraunces_b.cmap[ord("B")]].draw(bp)
    xmin, ymin, xmax, ymax = bp.bounds
    bx = -s_ * (xmin + xmax) / 2.0
    by = s_ * (ymin + ymax) / 2.0
    body, _ = fraunces_b.body("B", size, bx, by, b_fill, 0)
    g.append(body)
    g.append(sparkle(126, -126, 23, "#FFFFFF" if not mono else mono, 0.92))
    g.append("</g>")
    return "".join(g)


def icon(size=512, x=0, y=0, mono=None):
    k = size / 512.0
    g = [f'<g transform="translate({x} {y}) scale({k})">']
    if not mono:
        g.append('<rect x="6" y="6" width="500" height="500" rx="118" fill="url(#tile)"/>')
        g.append(
            '<rect x="6" y="6" width="500" height="500" rx="118" fill="none" '
            'stroke="#FFFFFF" stroke-opacity="0.07" stroke-width="3"/>'
        )
        g.append(
            '<rect x="6" y="6" width="500" height="500" rx="118" fill="url(#tileGlow)"/>'
        )
    g.append(mark(mono=mono))
    g.append("</g>")
    return "".join(g)


def horizontal(text_color, muted_color, kicker_color=None, mono=None,
               word_grad="wgrad"):
    pad = 8
    icon_size = 132
    gap = 30
    name_size = 62
    kicker_size = 17

    tx = pad + icon_size + gap
    name_y = pad + 118
    kicker_y = pad + 50
    kicker_color = kicker_color or muted_color

    if mono:
        body, word_w = fraunces.body(WORD, name_size, tx, name_y, mono, WORD_TRACK)
    else:
        b1, w1 = fraunces.body("Beauty", name_size, tx, name_y, f"url(#{word_grad})", WORD_TRACK)
        b2, w2 = fraunces.body(" Filter", name_size, tx + w1, name_y, text_color, WORD_TRACK)
        body, word_w = b1 + b2, w1 + w2
    kicker, kicker_w = manrope_b.body("OBS", kicker_size, tx + 2, kicker_y,
                                      kicker_color, 0.36)

    width = int(tx + max(word_w, kicker_w) + pad + 10)
    height = icon_size + 2 * pad

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}" role="img" aria-label="OBS Beauty Filter">',
        DEFS,
        icon(icon_size, pad, pad, mono=mono),
        kicker,
        body,
        "</svg>",
    ]
    return "\n".join(parts)


def stacked(text_color, muted_color, kicker_color=None, mono=None,
            word_grad="wgrad"):
    size, icon_size, name_size = 600, 196, 58
    icon_x = (size - icon_size) / 2
    kicker_color = kicker_color or muted_color

    word_w = fraunces.width_units(WORD, WORD_TRACK) * name_size / fraunces.upem
    kicker_w = manrope_b.width_units("OBS", 0.36) * 17 / manrope_b.upem

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="400" '
        f'viewBox="0 0 {size} 400" role="img" aria-label="OBS Beauty Filter">',
        DEFS,
        icon(icon_size, icon_x, 6, mono=mono),
        manrope_b.body("OBS", 17, (size - kicker_w) / 2, 268, kicker_color, 0.36)[0],
        (fraunces.body(WORD, name_size, (size - word_w) / 2, 340, mono, WORD_TRACK)[0]
         if mono else
         fraunces.body("Beauty", name_size, (size - word_w) / 2, 340, f"url(#{word_grad})", WORD_TRACK)[0]
         + fraunces.body(" Filter", name_size,
                         (size - word_w) / 2 + fraunces.width_units("Beauty", WORD_TRACK) * name_size / fraunces.upem,
                         340, text_color, WORD_TRACK)[0]),
        "</svg>",
    ]
    return "\n".join(parts)


def banner():
    w, h = 1280, 640
    icon_size = 180
    gap = 52
    kicker_size = 20
    name_size = 66
    sub_size = 24

    word_w = fraunces.width_units(WORD, WORD_TRACK) * name_size / fraunces.upem
    kicker_w = manrope_b.width_units("OBS", 0.36) * kicker_size / manrope_b.upem
    sub_w = manrope_sb.width_units("Real-time GPU beauty for OBS Studio", 0.01) * sub_size / manrope_sb.upem

    text_w = max(word_w, kicker_w, sub_w)
    group_w = icon_size + gap + text_w
    icon_x = (w - group_w) / 2
    tx = icon_x + icon_size + gap
    icon_y = (h - icon_size) / 2

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
        f'viewBox="0 0 {w} {h}" role="img" aria-label="OBS Beauty Filter">',
        DEFS,
        f'<rect width="{w}" height="{h}" fill="url(#tile)"/>',
        f'<rect width="{w}" height="{h}" fill="url(#bgGlowA)"/>',
        f'<rect width="{w}" height="{h}" fill="url(#bgGlowB)"/>',
        icon(icon_size, icon_x, icon_y),
        manrope_b.body("OBS", kicker_size, tx + 2, 270, ROSE, 0.36)[0],
        fraunces.body("Beauty", name_size, tx, 344, "url(#wgrad)", WORD_TRACK)[0],
        fraunces.body(" Filter", name_size,
                      tx + fraunces.width_units("Beauty", WORD_TRACK) * name_size / fraunces.upem,
                      344, TEXT, WORD_TRACK)[0],
        f'<rect x="{tx + 2:.1f}" y="372" width="74" height="4" rx="2" fill="url(#accent)"/>',
        manrope_sb.body("Real-time GPU beauty for OBS Studio", sub_size, tx, 418,
                        MUTED, 0.01)[0],
        "</svg>",
    ]
    return "\n".join(parts)


FILES = {
    "icon.svg": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" '
        'viewBox="0 0 512 512" role="img" aria-label="OBS Beauty Filter">'
        + DEFS + icon(512, 0, 0) + "</svg>"
    ),
    "mark.svg": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" '
        'viewBox="0 0 512 512" role="img" aria-label="OBS Beauty Filter">'
        + DEFS + mark() + "</svg>"
    ),
    "logo-horizontal-dark.svg": horizontal(TEXT, LIGHT_MUTED, ROSE),
    "logo-horizontal-light.svg": horizontal(LIGHT_TEXT, LIGHT_MUTED, ROSE, word_grad="wgradL"),
    "logo-horizontal-mono-white.svg": horizontal("#FFFFFF", "#FFFFFF", "#FFFFFF", mono="#FFFFFF"),
    "logo-horizontal-mono-black.svg": horizontal("#0B1020", "#0B1020", "#0B1020", mono="#0B1020"),
    "logo-stacked-dark.svg": stacked(TEXT, LIGHT_MUTED, ROSE),
    "logo-stacked-light.svg": stacked(LIGHT_TEXT, LIGHT_MUTED, ROSE, word_grad="wgradL"),
    "logo-stacked-mono-white.svg": stacked("#FFFFFF", "#FFFFFF", "#FFFFFF", mono="#FFFFFF"),
    "logo-stacked-mono-black.svg": stacked("#0B1020", "#0B1020", "#0B1020", mono="#0B1020"),
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
