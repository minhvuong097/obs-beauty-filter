# Kiểm thử

**[English](TESTING.en.md) | Tiếng Việt**

## 1. Build kiểm tra

### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBEAUTY_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
```

### Windows

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DBEAUTY_BUILD_TESTS=ON
cmake --build build --config RelWithDebInfo
```

Nếu dùng Visual Studio 2022, thay generator bằng `"Visual Studio 17 2022"`.
Trên Windows, `BEAUTY_OBS33_PLUGIN_LAYOUT` mặc định `ON` và staging dùng layout
OBS 33+. Thêm `-DBEAUTY_OBS33_PLUGIN_LAYOUT=OFF` nếu cần test layout legacy cho
OBS 32 trở xuống.

## 2. Face-region unit test

Test thuần CPU, không cần graphics:

```bash
./build/beauty-face-region-test
```

Kỳ vọng:

```text
OK: face region detection tests passed
```

Test tạo grid skin mask tổng hợp (mặt cao hơn rộng, có lỗ mắt/miệng và một mảng tường màu da), sau đó kiểm tra:

1. Detector trả về box hợp lệ.
2. Tâm box gần tâm mặt thật.
3. Box không bị mảng tường/da nhỏ ngoài rìa kéo lệch.
4. Grid rỗng hoặc quá nhỏ trả về `valid = false`.

## 3. Preset unit test

Test thuần CPU, không cần graphics:

```bash
./build/beauty-preset-test
```

Kỳ vọng:

```text
OK: preset table, lookup, apply, defaults and NULL handling verified
```

Test kiểm tra số lượng preset và ID duy nhất, khoảng giá trị hợp lệ, chuỗi quality, hành vi `find()` với `custom`/ID lạ, việc `apply()` ghi đủ mọi khóa mà không đè lên lựa chọn preset, preset `default` khớp với `beauty_defaults()`, và tham số `NULL` được bỏ qua.

## 4. Load + render integration test

Test tạo filter `beauty_filter`, gắn vào source và render qua `gs_texrender`. Nếu effect sai cú pháp hoặc thiếu file data, OBS có thể vẫn trả về một source object nhưng `obs_obj_invalid()` là true; test kiểm tra điều này và fail ngay. OBS 30.2 cố ý giữ source object không hợp lệ để không làm mất settings của người dùng, nên không thể chỉ kiểm tra con trỏ `NULL`.

### Linux

```bash
xvfb-run -a ./build/beauty-load-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter \
  /usr/lib/obs-plugins/image-source.so \
  /usr/share/obs/obs-plugins/image-source
```

Kỳ vọng (giá trị có thể xê dịch ±1–2 do làm tròn):

```text
center pixel RGBA (original):   200 150 120 255
center pixel RGBA (processed):  201 151 121 255
center pixel RGBA (skin mask):  255 255 255 255
center pixel RGBA (opacity 0):  200 150 120 255
pattern eye/mouth luma before:  215 / 198
pattern eye/mouth luma after:   243 / 204
zone preview eye RGBA:          109 183 255 255
zone preview mouth RGBA:        244 251 98 255
HDR pixel before filter:        4.000 0.400 0.400 1.000
HDR pixel after filter:         4.078 0.420 0.435 1.000
scRGB pixel before filter:      4.000 0.400 0.400 1.000
scRGB pixel after filter:       4.227 0.457 0.500 1.000
texture stripe contrast before: 10 (161 vs 151)
texture stripe contrast after:  1 (157 vs 156)
texture stripe contrast detail: 8 (160 vs 152)
OK: module loaded, filter created, effect compiled, filter rendered, mask/opacity/face zones/texture/HDR verified
```

Test kiểm tra:

1. **Filter mặc định** chạy trên màu da `RGB(200,150,120)` và giữ pixel gần giá trị gốc (sai số ±8 do rosy/tone mặc định).
2. **Show skin mask** cho ra trắng hoàn toàn ở vùng da phẳng.
3. **Opacity = 0** trả lại chính xác frame gốc (sai số ±1 do làm tròn).
4. Alpha của output vẫn là 255.
5. **Teeth/eye zones**: dùng pattern tổng hợp có mặt, mắt xám và miệng xám; bật `face_manual` với box phủ đúng mặt.
   - Mouth luma tăng ít nhất 6 (thực tế 198 → 204).
   - Eye luma tăng ít nhất 15 (thực tế 215 → 243).
6. **Zone preview**: mắt phải ngả xanh dương (`B > R + 40`), miệng phải vàng (`R > 180` và `R > B + 30`).
7. **Frequency separation**: pattern da tổng hợp có stripe 10 px xen kẽ hai tông da gần nhau, render hai lần.
   - Với `Amount=1`, `Radius=24`, `Detail=0`, `skin_detect=on`, contrast stripe phải giảm còn tối đa 55% giá trị gốc (thực tế 10 → 1–2).
   - Với `Detail=0.9`, cùng filter phải giữ thêm ít nhất 3 mức contrast so với `Detail=0` (thực tế 8), chứng minh detail control hoạt động chứ không phải blur thuần.
8. **HDR Rec.709 extended**: pattern vẽ full frame `(4.0, 0.4, 0.4)`; sau filter:
   - `R > 1.5` → highlight không bị clip về SDR white.
   - `R > before + 0.05` → nguồn thực sự được xử lý, không bị bypass.
9. **HDR scRGB**: pattern báo `GS_CS_709_SCRGB` và vẽ cùng giá trị `(4.0, 0.4, 0.4)`:
   - `R > 1.5` và `R > before + 0.05` như trên.
   - `R < before * 1.5` → phát hiện lỗi scale (hdr_scale thiếu/thừa; lỗi cũ cho ra ~15 thay vì ~4.4).

Nếu ra `0,0,0,0`, filter creation fail, hoặc sai lệch màu lớn, xem log shader bên dưới.

### Windows

```powershell
.\build\RelWithDebInfo\beauty-face-region-test.exe
.\build\RelWithDebInfo\beauty-preset-test.exe

# Layout OBS 33+ (mặc định trên Windows)
.\build\RelWithDebInfo\beauty-load-test.exe `
  .\build\RelWithDebInfo\obs-beauty-filter.dll `
  .\build\rundir\plugins\obs-beauty-filter\data `
  "C:\Program Files\obs-studio\obs-plugins\64bit\image-source.dll" `
  "C:\Program Files\obs-studio\data\obs-plugins\image-source"

# Layout legacy (chỉ khi cấu hình -DBEAUTY_OBS33_PLUGIN_LAYOUT=OFF)
.\build\RelWithDebInfo\beauty-load-test.exe `
  .\build\RelWithDebInfo\obs-beauty-filter.dll `
  .\build\rundir\data\obs-plugins\obs-beauty-filter `
  "C:\Program Files\obs-studio\obs-plugins\64bit\image-source.dll" `
  "C:\Program Files\obs-studio\data\obs-plugins\image-source"
```

> Tham số thứ ba là thư mục **data**, không phải thư mục chứa DLL. Với layout
> OBS 33+, data nằm ở `build\rundir\plugins\obs-beauty-filter\data`.
>
> Đường dẫn `image-source` có thể khác tùy bản OBS. Trên OBS 33, module core
> này có thể nằm trong thư mục `core` của bản cài hoặc build OBS; hãy tìm
> `image-source.dll` và thư mục data tương ứng. Nếu không truyền, test chỉ kiểm
> tra load + compile effect.

## 5. Ghi chú orientation khi đọc pixel

Test integration dùng `gs_stage_texture()` + `gs_stagesurface_map()` để đọc pixel. Trên OpenGL/llvmpipe trong môi trường CI hiện tại, hàng đầu tiên map ra là **hàng trên** của ảnh (top-down), nên `render_pixel()` dùng `row = y` và pattern vẽ với gốc toạ độ top-based (`translate(x, y)`).

Nếu chạy trên backend trả về bottom-up (một số driver/backend có thể khác), các giá trị `pattern eye/mouth luma` sẽ lệch và test sẽ fail ở bước zone. Khi đó cần kiểm tra lại quy ước của backend thay vì sửa ngưỡng test.

## 6. Kiểm tra log OBS

Mở **Help → Log Files → View Current Log** và tìm:

- `Loading module: obs-beauty-filter` — module được load.
- `Failed to load '...beauty_filter.effect'` — **không** được xuất hiện.
- `Compiler warnings/errors for ...beauty_filter.effect` — **không** được xuất hiện.
- `Source ID 'beauty_filter' not found` — **không** được xuất hiện sau khi thêm filter.
- `[obs-beauty-filter] Failed to allocate GPU resources` — **không** được xuất hiện.

Khi thêm filter lần đầu, OBS sẽ compile cả 6 technique (`MaskPass`, `MaskAnalysis`, `BlurBase`, `DrawFast`, `DrawBalanced`, `DrawHigh`); lỗi ở bất kỳ technique nào cũng làm effect load fail.

## 7. Test matrix thủ công

| # | Trường hợp | Kỳ vọng |
|---|---|---|
| 1 | Thêm filter vào camera 1080p | Filter xuất hiện, ảnh mượt nhẹ |
| 2 | Amount = 0, Opacity = 1 | Ảnh gần như gốc (tone adjustment = 0 nếu các slider tone = 0) |
| 3 | Opacity = 0 | Ảnh gốc hoàn toàn, kể cả khi Amount > 0 |
| 4 | Show skin mask | Da trắng, mắt/môi/tóc/nền đen |
| 5 | Show face zones | Face prior xanh lá, mắt xanh dương, miệng vàng, khớp mặt |
| 6 | Split preview | Nửa trái gốc, nửa phải đã xử lý |
| 7 | Fast / Balanced / High | Không lỗi shader, chất lượng tăng dần |
| 8 | Radius 0 | Không làm mịn (chỉ còn tone/clarity) |
| 9 | Radius 24 @1080p | Mịn rõ, không crash, FPS giảm |
| 10 | Nguồn 720p / 1440p / 4K | Bán kính tương đương, không lỗi |
| 11 | Nguồn có alpha (PNG/scene) | Viền alpha giữ đúng, không quầng đen |
| 12 | Nguồn HDR Rec.709 extended | Filter xử lý, highlight > 1 giữ nguyên, không bypass |
| 13 | Nguồn HDR scRGB | Filter xử lý, highlight > SDR white giữ nguyên, không scale hai lần |
| 14 | Teeth = 0.5, Eyes = 0 | Răng sáng hơn, môi/da không bị trắng |
| 15 | Eyes = 0.3 | Lòng trắng sáng hơn, da quanh mắt không quá sáng |
| 16 | Manual face box | Box thủ công thay auto; zones đi theo box |
| 17 | Temporal = 0 / 0.6 / 0.9 | 0 = flicker hơn; 0.9 = mượt nhưng có thể ghosting khi quay nhanh |
| 18 | Xóa filter khi đang render | Không crash |
| 19 | Đổi settings liên tục | Không leak, không nhấp nháy bất thường |
| 20 | Stream/record 10 phút | Không tăng RAM/VRAM, không drop frame |
| 21 | Da có texture, Detail 0 → 1 | Detail 0 làm mịn; Detail 1 trả lại lỗ chân lông/nếp nhăn nhỏ |
| 22 | Gradient mượt, ngõ ra 8-bit | Anti-banding on: không thấy dải; off: dải quay lại |
| 23 | Camera cao cấp 1440p/4K, Quality High | Da mịn nhưng còn chi tiết, không halo, không bệt |

## 8. Kiểm tra hiệu năng

1. Dùng **Stats** của OBS (View → Stats) để xem FPS và dropped frames.
2. So sánh 3 mức Quality với cùng scene.
3. So sánh Face-aware On/Off (mask pass + readback nhỏ).
4. Nếu GPU load > 90% hoặc FPS tụt:
   - hạ Quality;
   - giảm Radius;
   - tắt Glow;
   - tắt Face-aware nếu không dùng teeth/eye;
   - kiểm tra filter khác (denoise, sharpen) đang chạy.

Không có CPU readback full-frame trong plugin. Chi phí CPU chỉ là grid 64×N (tối đa 16 KB) mỗi 6 frame khi auto face-aware bật.

## 9. Kiểm tra chất lượng hình ảnh

- Chụp screenshot trước/sau với cùng ánh sáng, cùng frame.
- Kiểm tra ở 100% zoom các vùng: mắt, lông mày, viền môi, tóc mai, đường viền hàm.
- Kiểm tra da ở vùng sáng nhất và tối nhất để tránh mất chi tiết.
- Kiểm tra teeth/eye zones ở mặt nhìn thẳng, mặt quay 3/4 và khi có vật che.
- Với nhiều tông da, chụp ít nhất 3 mức sáng/tối khác nhau và xác nhận mask + face box.
- Với HDR, kiểm tra highlight (đèn, cửa sổ) trước/sau để chắc không bị clip.

## 10. Test trên Windows cụ thể

- Windows 10/11, OBS 30.2+ (64-bit); ưu tiên xác nhận trên OBS 33.0.1 trở lên.
- GPU Intel iGPU, NVIDIA, AMD.
- Kiểm tra DLL load bằng **Dependencies** hoặc **Process Monitor** nếu OBS không thấy filter.
- Layout OBS 33+: DLL và data phải nằm trong cùng một thư mục plugin:
  `%ProgramData%\obs-studio\plugins\obs-beauty-filter\obs-beauty-filter.dll`
  và `%ProgramData%\obs-studio\plugins\obs-beauty-filter\data\`.
  Với OBS portable, thay `%ProgramData%\obs-studio\plugins` bằng
  `<thư-mục-OBS-portable>\plugins`.
- Layout legacy (OBS 32 trở xuống): DLL trong `obs-plugins\64bit`, data trong
  `data\obs-plugins\obs-beauty-filter`. OBS 33 vẫn load nhưng gắn nhãn Legacy và
  OBS 34 sẽ ngừng load layout này.
- Nếu OBS 33 bật **Safe Mode**, plugin bên thứ ba sẽ không được load; hãy thoát
  Safe Mode trước khi kiểm tra.
- Chạy `beauty-face-region-test.exe`, `beauty-preset-test.exe` và `beauty-load-test.exe`; kiểm tra cả HDR Rec.709 extended và scRGB trên GPU thật.
- Test headless đã bao phủ scRGB; readback 16F vẫn có thể khác giữa D3D11 và OpenGL nên cần xác nhận lại bằng nguồn/canvas HDR thực tế.
- Khi sửa effect, tránh constructor vector một tham số như `float3(x)`: FXC của libobs-d3d11 báo lỗi `X3014: incorrect number of arguments to numeric-type constructor`. Hãy viết `float3(x, x, x)`. Compiler Linux/OpenGL chấp nhận cả hai nên chỉ CI Windows mới phát hiện được lỗi này.

## 11. CI gợi ý

Có thể chạy job Linux:

```yaml
- cmake -S . -B build -DBEAUTY_BUILD_TESTS=ON
- cmake --build build -j
- ./build/beauty-face-region-test
- ./build/beauty-preset-test
- xvfb-run -a ./build/beauty-load-test ...
```

Windows CI cần OBS deps + libobs package; dùng cùng quy trình với
`obs-plugintemplate`. CI của repo này build OBS 33.0.1 từ source với
`obs-deps` và chạy `beauty-load-test` trên D3D11 (load module + compile effect)
trước khi đóng gói. Matrix render/mask/HDR đầy đủ vẫn chạy trên Linux/OpenGL;
giữ matrix thủ công ở mục 7 để xác nhận theo từng GPU.
