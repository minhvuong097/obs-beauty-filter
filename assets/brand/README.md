# OBS Beauty Filter — brand assets

The visual identity for **OBS Beauty Filter**: a six-blade camera aperture whose
iris opening turns into a beauty sparkle.

| | |
|---|---|
| Mark concept | A lens aperture (six rounded blades) with a glowing iris core and a highlight sparkle — the "beauty filter" moment where a camera lens becomes light. |
| Personality | Precise, soft, modern. Studio tooling, not toy software. |
| Wordmark | **Manrope** ExtraBold, tight tracking; `OBS` kicker in Bold with wide tracking. |

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
| Rose (primary) | `#FF5C9E` | Left end of the aperture gradient, accents |
| Peach (secondary) | `#FFB36B` | Right end of the gradient, iris glow |
| Ink | `#0B1020` | Dark background / mono on light |
| Ink raised | `#1A2340` | Tile gradient end |
| Text on dark | `#F7F8FC` | Wordmark, dark theme |
| Muted on dark | `#93A5C7` | `OBS` kicker, subtitles |
| Text on light | `#0D1424` | Wordmark, light theme |
| Muted on light | `#5B6B8C` | `OBS` kicker, subtitles |

The aperture gradient runs from `#FF6FA8` through `#FF5C9E` to `#FFB36B` on a
`-170,-180 → 170,180` diagonal. Keep it diagonal; do not flatten it to one colour
outside the mono variants.

## Clearspace and minimum size

- Clearspace on all sides: **25% of the mark's height**. Nothing (text, borders,
  other logos) enters that zone.
- Minimum sizes: icon **16 px**; horizontal lockup **120 px** wide; stacked
  lockup **96 px** wide. Below that, use the icon alone.
- The tile corner radius is `118/512` (≈23%) of the tile. Keep the mark centred
  and do not re-round, rotate or outline the tile.

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
`assets/brand/png/`. All wordmark text is converted to paths, so the SVGs render
identically without the font installed.

## Licence

The source code is GPL-2.0-or-later. The brand assets (name, logo, icon,
lockups, banner) are **not** covered by the GPL: they are © 2026
minhvuong097, and you may use them unmodified to refer to this project
(screenshots, reviews, tutorials, "works with OBS Beauty Filter" statements).
Any other use, modified versions, or derived marks need written permission.
Manrope is licensed under the SIL Open Font License 1.1 — see
`tools/brand/fonts/OFL.txt`.

---

## Tiếng Việt

Logo là **khẩu độ sáu lá** của ống kính, phần mống mắt phát sáng thành tia
sparkle — ẩn dụ "ống kính biến thành làn da đẹp".

- Nền tối dùng `logo-horizontal-dark` / `logo-stacked-dark`; nền sáng dùng bản
  `light`; in một màu dùng bản `mono`.
- Màu chủ đạo: hồng `#FF5C9E` → đào `#FFB36B` trên nền `#0B1020`.
- Khoảng trống quanh logo tối thiểu **25% chiều cao mark**; icon nhỏ nhất 16 px,
  lockup ngang nhỏ nhất 120 px.
- Không đổi màu chữ, không bóp méo, không thêm hiệu ứng, không đặt trên ảnh nền
  rối mà không có lớp nền.
- Bộ nhận diện (tên, logo, icon) **không** thuộc GPL; được dùng nguyên bản để
  nhắc tới dự án. Sửa đổi hoặc dùng cho sản phẩm khác cần xin phép.
