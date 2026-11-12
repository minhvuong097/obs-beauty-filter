# OBS Beauty Filter — brand assets

The visual identity for **OBS Beauty Filter**: a six-petal aperture flower that
doubles as a camera iris, paired with the full product name on **one line**.

| | |
|---|---|
| Mark concept | Six rounded petals in spectrum order (`#FF7AB8` → `#B14CF2`) on a deep-violet tile, a dark iris core and a white sparkle. A camera aperture that blooms — the "beauty filter" moment. |
| Personality | Colourful, precise, friendly. Creative tooling for streamers, not clinical software. |
| Wordmark | The full name, always on a single line next to the mark: `OBS` in **Manrope Bold** at a muted tone, `Beauty Filter` in **Manrope ExtraBold**, tight tracking. Same size, two tones — one name, one line. |

## Files

```
assets/brand/
├── svg/                              # source of truth, text converted to paths
│   ├── icon.svg                      # app icon, 512×512 tile
│   ├── mark.svg                      # flower only, no tile
│   ├── logo-horizontal-{dark,light}.svg
│   ├── logo-horizontal-mono-{white,black}.svg
│   ├── logo-stacked-{dark,light}.svg
│   ├── logo-stacked-mono-{white,black}.svg
│   └── banner.svg                    # 1280×640 hero / social preview
└── png/                              # convenience exports of the SVGs
    ├── icon-{512,256,128,64,48,32,16}.png
    ├── icon.ico                      # 16–256 px, used by the Windows installer
    ├── logo-horizontal-{dark,light,mono-white,mono-black}.png
    ├── logo-stacked-{dark,light}.png
    └── banner.png / social-preview.png
```

`dark` variants have light text and are for dark backgrounds; `light` variants
have dark text and are for light backgrounds. `mono-*` variants drop the tile and
render the flower in a single colour for stamps, watermarks, embroidery and other
one-colour reproduction.

## Layout

The horizontal lockup is the primary form: tile, then the name on one baseline.

| Property | Value | Notes |
|---|---|---|
| Icon | 116 px | 1.86× the wordmark cap height |
| Gap | 26 px | optical, not a full space |
| Text size | 58 px | both `OBS` and `Beauty Filter` |
| Vertical alignment | cap height centred on the tile | the name sits dead centre of the tile |

Never stack `OBS` above `Beauty Filter` in the horizontal lockup, and never split
the name onto two lines — the single line is the identity.

## Colour

| Role | Hex | Notes |
|---|---|---|
| Tile start | `#1D1145` | Top-left of the tile gradient |
| Tile end | `#0A0720` | Bottom-right, also the mono ink |
| Core | `#120C2A` | Iris centre behind the sparkle |
| Petal 1 | `#FF7AB8` | Pink, top |
| Petal 2 | `#FF9A62` | Peach |
| Petal 3 | `#FFD36B` | Gold |
| Petal 4 | `#5EEAD4` | Mint |
| Petal 5 | `#6EA8FF` | Blue |
| Petal 6 | `#B14CF2` | Violet |
| Text on dark | `#F7F8FC` | `Beauty Filter` |
| Muted on dark | `#A9B3D6` | `OBS`, subtitles |
| Text on light | `#0D1424` | `Beauty Filter` |
| Muted on light | `#5B6B8C` | `OBS`, subtitles |

Petal order is clockwise from the top and must not be shuffled: the sequence is
what makes the mark read as a spectrum while staying balanced. Do not add new
petal colours or flatten the tile gradient outside the mono variants.

## Clearspace and minimum size

- Clearspace on all sides: **25% of the tile height**. Nothing (text, borders,
  other logos) enters that zone.
- Minimum sizes: icon **16 px**; horizontal lockup **150 px** wide; stacked
  lockup **110 px** wide. Below that, use the icon alone.
- The tile corner radius is `46/184` (25%) of the tile. Keep it; never re-round,
  rotate, crop or outline the tile.
- The sparkle is a highlight, not a separate logo: it must stay white on the dark
  core and must not be enlarged or moved outside the flower.

## Usage

Do:

- Use `logo-horizontal-{dark,light}` in README headers, slides and website
  headers; `logo-stacked-*` where the layout is square or narrow.
- Prefer SVG; use PNG only where SVG is not supported.
- Keep the supplied gap between the tile and the wordmark when composing your
  own layout (the shipped lockups already use the correct spacing).

Don't:

- Don't recolour the wordmark, add drop shadows, glows or outlines.
- Don't separate `OBS` from `Beauty Filter` with a rule, pill, or line break.
- Don't place the lockup on busy photography without a solid or blurred backing.
- Don't stretch, skew, rotate or crop the mark.
- Don't use the mark to imply endorsement of an unrelated product. See the
  licence note below.

## Regenerating

```sh
python3 tools/brand/gen.py        # needs fontTools; rsvg-convert for PNGs
```

The generator writes `assets/brand/svg/` and, when `rsvg-convert` is installed,
`assets/brand/png/` (including the multi-resolution `icon.ico`). All wordmark
text is converted to paths, so the SVGs render identically without the font
installed. Fonts in `tools/brand/fonts/`: Manrope Bold/SemiBold/ExtraBold.

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

Logo là **hoa khẩu độ sáu cánh** nhiều màu trên nền tím than, ở giữa là mống mắt
tối với tia sáng trắng; tên đầy đủ "OBS Beauty Filter" luôn nằm **một dòng** bên
phải logo, set bằng Manrope: `OBS` chữ Bold màu dịu, `Beauty Filter` chữ
ExtraBold màu trắng, cùng cỡ chữ.

- Tên đầy đủ luôn xuất hiện trên một dòng: `OBS` + `Beauty Filter`, không rút gọn
  thành "BF", không xuống dòng, không chèn gạch ngăn cách.
- Tỉ lệ chuẩn: icon 116 px, khoảng cách 26 px, chữ 58 px; chữ canh giữa theo
  chiều cao cap của icon.
- Nền tối dùng `logo-horizontal-dark` / `logo-stacked-dark`; nền sáng dùng bản
  `light`; in một màu dùng bản `mono` (bỏ nền tile, hoa vẽ một màu).
- Thứ tự cánh hoa theo chiều kim đồng hồ từ trên xuống:
  hồng `#FF7AB8`, cam đào `#FF9A62`, vàng `#FFD36B`, mint `#5EEAD4`,
  xanh `#6EA8FF`, tím `#B14CF2`. Không đổi thứ tự, không thêm màu.
- Khoảng trống quanh logo tối thiểu **25% chiều cao tile**; icon nhỏ nhất 16 px,
  lockup ngang nhỏ nhất 150 px.
- Không đổi màu chữ, không bóp méo, không thêm hiệu ứng, không đặt trên ảnh nền
  rối mà không có lớp nền.
- Bộ nhận diện (tên, logo, icon) **không** thuộc GPL; được dùng nguyên bản để
  nhắc tới dự án. Sửa đổi hoặc dùng cho sản phẩm khác cần xin phép.
