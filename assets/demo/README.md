# OBS Beauty Filter — demo assets

The stills and the GIF in this folder are **real output of the plugin**, not
mock-ups. Each frame is rendered by `tools/demo/render-demo.c`, which starts a
headless libobs graphics context, loads the built plugin and an
`image_source`, feeds it the photo below and reads the processed frame back.
The same renderer is the only source of pixels; ImageMagick and ffmpeg only
add the captions and assemble the GIF.

## Files

| File | What it shows |
|---|---|
| `before-after.jpg` | 1024×683 camera frame, original vs. filter at Amount 0.62, Light balance 0.45 |
| `skin-detail.png` | 100% crop at 200% zoom, same region both sides: pore and hair detail is kept while the skin evens out |
| `amount-slider.gif` | Ping-pong sweep of **Amount** from 0.00 to 0.90 with the other controls fixed |
| `obs-panel.png` | Real OBS 30.2 "Filters" dialog with the plugin property page open (Xvfb + openbox, llvmpipe) |
| `source-photo.jpg` | Unmodified CC0 input used by the renderer |

`amount-slider.gif` and both comparison stills use the plugin defaults for
Radius (9 px @1080p), Detail (0.40), Light balance (0.45), Rosy tint (0.12) and
Clarity (0.08), so the GIF isolates the smoothing amount.

## Regenerating

```sh
cmake -S . -B build -DBEAUTY_BUILD_DEMO_TOOLS=ON
cmake --build build --target beauty-demo-render

BEAUTY_PLUGIN=build/obs-beauty-filter.so \
BEAUTY_PLUGIN_DATA=build/rundir/share/obs/obs-plugins/obs-beauty-filter \
IMAGE_SOURCE_PLUGIN=/usr/lib/obs-plugins/image-source.so \
IMAGE_SOURCE_DATA=/usr/share/obs/obs-plugins/image-source \
    tools/demo/build-assets.sh
```

`build-assets.sh` needs ImageMagick 7 and gifski (it falls back to ffmpeg's
`palettegen`/`paletteuse` when gifski is missing); on a headless machine it runs
the renderer under `xvfb-run` automatically. The version in the "After" caption
is read from `CMakeLists.txt`, so the images never advertise a stale release. `obs-panel.png` needs a real OBS
window, so the script only replaces it when `OBS_PANEL=/path/to/shot.png` is
set.

The rendered frames are genuine filter output on whatever GPU the renderer
picks. On a headless CI box that is Mesa llvmpipe, which matches the OpenGL
path users run on Linux; the smoothing maths is shared with the D3D11 path.

## Credit

`source-photo.jpg` is **"Autumn Girl"** by
[jonas.svidras](https://www.flickr.com/photos/140330936@N02/40961572442),
released under [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/).
The file is unmodified; the CC0 dedication waives copyright and related rights,
so it is not covered by this project's GPL-2.0-or-later licence either.

The person in the photo is not associated with this project and does not
endorse it. The image is used only to demonstrate a skin-processing filter. If
you are the person depicted and want the image removed, open an issue and it
will be replaced.

## Licence

The demo images combine GPL-2.0-or-later software output with a CC0 photo and
are published under the same terms as the rest of `assets/`: the plugin output
may be reused to describe this project, the photo stays CC0.

---

## Tiếng Việt

Ảnh và GIF trong thư mục này là **kết quả thật của plugin**, không phải mock-up.
Từng khung hình do `tools/demo/render-demo.c` render qua libobs headless, nạp
plugin đã build cùng `image_source`, đưa ảnh gốc vào rồi đọc lại khung đã xử lý.
ImageMagick và ffmpeg chỉ thêm nhãn và ghép GIF.

| Tệp | Nội dung |
|---|---|
| `before-after.jpg` | Khung 1024×683: ảnh gốc vs. filter với Amount 0.62, Light balance 0.45 |
| `skin-detail.png` | Crop 100% phóng 200%, cùng vùng hai bên: giữ chi tiết lỗ chân lông và tóc trong khi da phẳng màu hơn |
| `amount-slider.gif` | Quét **Amount** 0.00 → 0.90 rồi chạy ngược, các thông số khác giữ nguyên |
| `obs-panel.png` | Hộp thoại "Filters" thật của OBS 30.2 với trang thuộc tính plugin (Xvfb + openbox, llvmpipe) |
| `source-photo.jpg` | Ảnh CC0 gốc, không chỉnh sửa |

Tạo lại bằng `tools/demo/build-assets.sh` (cần ImageMagick 7 và gifski, thiếu
gifski thì dùng ffmpeg; máy không có màn hình thì script tự chạy qua `xvfb-run`).
Số phiên bản trên nhãn "After" đọc từ `CMakeLists.txt` nên ảnh không bị lỗi thời. `obs-panel.png` cần cửa sổ OBS
thật nên chỉ được thay khi đặt `OBS_PANEL=/đường/dẫn/ảnh.png`.

**Ghi công:** `source-photo.jpg` là ảnh **"Autumn Girl"** của
[jonas.svidras](https://www.flickr.com/photos/140330936@N02/40961572442),
phát hành theo [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/),
không chỉnh sửa. Người trong ảnh không liên quan và không bảo trợ dự án; ảnh chỉ
dùng để minh hoạ filter làm đẹp da. Nếu bạn là người trong ảnh và muốn gỡ, hãy mở
issue, ảnh sẽ được thay.