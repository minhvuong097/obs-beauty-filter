# OBS Beauty Filter — brand assets

The visual identity for **OBS Beauty Filter**: an editorial serif wordmark
anchored by a **B** monogram, with a rose-to-peach finish and a beauty sparkle.

| | |
|---|---|
| Mark concept | A high-contrast serif **B** (Beauty) in a rose→peach gradient on a deep-navy tile, finished with a four-point sparkle. Elegant first, technical second. |
| Personality | Editorial, warm, premium. A beauty brand that happens to be a GPU filter. |
| Wordmark | **Fraunces** SemiBold with tight tracking: `Beauty` in the accent gradient, `Filter` in ink or white, `OBS` as a letterspaced **Manrope** Bold kicker. |

## Files

```
assets/brand/
├── svg/                              # source of truth, text converted to paths
│   ├── icon.svg                      # app icon, 512×512 tile
│   ├── mark.svg                      # mark only, transparent
│   ├── logo-horizontal-{dark,light}.svg
│   ├── logo-horizontal-mono-{white,black}.svg
│   ├── logo-stacked-{dark,light}.svg
│   ├── logo-stacked-mono-{white,black}.svg
│   └── banner.svg                    # 1280×640 hero / social preview
└── png/                              # convenience exports of the SVGs
    ├── icon-{512,256,128,64,48,32,16}.png
    ├── icon.ico                      # 16–256 px, used by the Windows installer
    ├── logo-horizontal-*.png
    ├── logo-stacked-*.png
    └── banner.png / social-preview.png
```

`dark` variants have light text and are for dark backgrounds; `light` variants
have dark text and are for light backgrounds. `mono-*` variants are single-colour
for stamps, watermarks, embroidery and other one-colour reproduction.

## Colour

| Role | Hex | Notes |
|---|---|---|
| Rose (primary) | `#FF5C9E` | Start of the monogram/wordmark gradient, accents |
| Peach (secondary) | `#FFB36B` | End of the gradient, warm highlights |
| Ink | `#0B1020` | Dark background / mono on light |
| Ink raised | `#1A2340` | Tile gradient end |
| Text on dark | `#F7F8FC` | Wordmark, dark theme |
| Muted on dark | `#93A5C7` | `OBS` kicker, subtitles |
| Text on light | `#0D1424` | Wordmark, light theme |
| Muted on light | `#5B6B8C` | `OBS` kicker, subtitles |

The accent gradient runs from `#FF5C9E` through `#FF7FA8` to `#FFB36B`. On the
monogram it follows a `120,1450 → 1450,-120` diagonal in glyph units; on the
`Beauty` wordmark it is horizontal. Keep it diagonal on the mark and do not
flatten it to one colour outside the mono variants.

## Clearspace and minimum size

- Clearspace on all sides: **25% of the mark's height**. Nothing (text, borders,
  other logos) enters that zone.
- Minimum sizes: icon **16 px**; horizontal lockup **120 px** wide; stacked
  lockup **96 px** wide. Below that, use the icon alone.
- The tile corner radius is `118/512` (≈23%) of the tile. Keep the monogram
  optically centred (the generator centres the glyph bounds, not the advance
  width) and do not re-round, rotate or outline the tile.
- The `OBS` kicker is set in caps with `0.36 em` tracking and sits roughly one
  cap-height above the wordmark. Do not lowercase it or merge it into the
  wordmark.

## Usage

Do:

- Use `logo-horizontal-{dark,light}` in README headers, slides and website
  headers; `logo-stacked-*` where the layout is square or narrow.
- Keep at least a 32 px gap between the icon and the wordmark when composing
  your own layout (the shipped lockups already use the correct spacing).
- Prefer SVG; use PNG only where SVG is not supported.

Don't:

- Don't recolour the wordmark, add drop shadows, gradients or outlines.
- Don't place the lockup on busy photography without a solid or blurred backing.
- Don't stretch, skew, rotate, or crop the mark.
- Don't use the mark to imply endorsement of an unrelated product. See the
  licence note below.

## Regenerating

```sh
python3 tools/brand/gen.py        # needs fontTools; rsvg-convert for PNGs
```

The generator writes `assets/brand/svg/` and, when `rsvg-convert` is installed,
`assets/brand/png/` (including the multi-resolution `icon.ico`). All wordmark
text is converted to paths, so the SVGs render identically without the fonts
installed. Fonts in `tools/brand/fonts/`: Fraunces SemiBold/Bold for the
monogram and wordmark, Manrope Bold/SemiBold for the kicker and taglines.

## Licence

The source code is GPL-2.0-or-later. The brand assets (name, logo, icon,
lockups, banner) are **not** covered by the GPL: they are © 2026
minhvuong097, and you may use them unmodified to refer to this project
(screenshots, reviews, tutorials, "works with OBS Beauty Filter" statements).
Any other use, modified versions, or derived marks need written permission.
Fraunces and Manrope are licensed under the SIL Open Font License 1.1 — see
`tools/brand/fonts/OFL-Fraunces.txt` and `tools/brand/fonts/OFL.txt`.

---

## Tiếng Việt

Logo là monogram **B** kiểu serif (Beauty) trên nền navy, tô gradient hồng→đào
và điểm một tia sparkle; tên đầy đủ "OBS Beauty Filter" được set bằng serif
Fraunces, với `OBS` là kicker chữ Manrope.

- Tên đầy đủ luôn xuất hiện: `OBS` + `Beauty Filter`, không rút gọn thành "BF"
  hay chỉ "Beauty".

- Nền tối dùng `logo-horizontal-dark` / `logo-stacked-dark`; nền sáng dùng bản
  `light`; in một màu dùng bản `mono`.
- Màu chủ đạo: hồng `#FF5C9E` → đào `#FFB36B` trên nền `#0B1020`.
- Khoảng trống quanh logo tối thiểu **25% chiều cao mark**; icon nhỏ nhất 16 px,
  lockup ngang nhỏ nhất 120 px.
- Không đổi màu chữ, không bóp méo, không thêm hiệu ứng, không đặt trên ảnh nền
  rối mà không có lớp nền.
- Bộ nhận diện (tên, logo, icon) **không** thuộc GPL; được dùng nguyên bản để
  nhắc tới dự án. Sửa đổi hoặc dùng cho sản phẩm khác cần xin phép.
