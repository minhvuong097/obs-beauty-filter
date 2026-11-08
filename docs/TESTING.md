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

### Sanitizer

AddressSanitizer và UndefinedBehaviorSanitizer là tùy chọn:

```bash
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DBEAUTY_BUILD_TESTS=ON -DBEAUTY_SANITIZE=address,undefined
cmake --build build-sanitize -j"$(nproc)"

ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
xvfb-run -a ./build-sanitize/beauty-load-test \
  ./build-sanitize/obs-beauty-filter.so \
  ./build-sanitize/rundir/share/obs/obs-plugins/obs-beauty-filter \
  /usr/lib/obs-plugins/image-source.so \
  /usr/share/obs/obs-plugins/image-source
```

Dùng `-DBEAUTY_SANITIZE=undefined` khi runtime ASan không map được shadow
memory (ví dụ trong một số container). Trên musl/Alpine, cấu hình với
`CC=clang`; GCC của Alpine không kèm `libasan`/`libubsan`. Tắt leak detection
vì libobs và Mesa giữ các allocation tồn tại đến hết process mà plugin không
sở hữu.

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
OK: built-in and custom presets, lookup, apply, defaults and NULL handling verified
```

Test kiểm tra số lượng preset và ID duy nhất, khoảng giá trị hợp lệ, chuỗi quality, hành vi `find()` với `custom`/ID lạ, việc `apply()` ghi đủ mọi khóa mà không đè lên lựa chọn preset, preset `default` khớp với `beauty_defaults()`, và tham số `NULL` được bỏ qua.

Phần custom preset ghi một file JSON thật qua `OBS_BEAUTY_CUSTOM_PRESET_PATH`,
đọc lại từ đĩa, rồi kiểm tra trim tên, ghi đè/xóa không phân biệt hoa thường,
từ chối tên trùng preset có sẵn hoặc tên dành riêng, từ chối ký tự điều khiển,
giới hạn độ dài và fallback về preset Default khi thiếu khóa. Ba file tạm
(`json`, `.tmp`, `.bak`) được dọn ở cuối.

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

### Render matrix / golden image

`beauty-render-matrix-test` render mọi preset có sẵn ở 360p, 720p, 1080p và
1440p, sau đó chạy một lượt 16F ở 720p. Test kiểm tra alpha luôn đục, mỗi preset
thực sự thay đổi frame, và giá trị 16F hữu hạn, không âm và giữ highlight trên
mức trắng SDR.

```bash
# Chỉ kiểm tra invariant (Linux và Windows)
xvfb-run -a ./build/beauty-render-matrix-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter

# So sánh golden image dạng lấy mẫu (CI Linux)
BEAUTY_MATRIX_GOLDEN=tests/golden/render-matrix.txt \
xvfb-run -a ./build/beauty-render-matrix-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter

# Tạo lại golden image sau khi cố ý thay đổi hình ảnh
BEAUTY_MATRIX_WRITE_GOLDEN=tests/golden/render-matrix.txt \
xvfb-run -a ./build/beauty-render-matrix-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter

# Bật 4K và xuất PPM để xem bằng mắt
BEAUTY_MATRIX_4K=1 BEAUTY_MATRIX_DUMP_DIR=/tmp/beauty-frames \
xvfb-run -a ./build/beauty-render-matrix-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter
```

Golden lưu màu trung bình của lưới 8×6, không lưu frame byte-exact, nên chịu
được sai số làm tròn của GPU mà vẫn bắt được thay đổi shader/preset.
`BEAUTY_MATRIX_TOLERANCE` đặt sai lệch tối đa mỗi kênh (mặc định 20). CI Windows
chạy invariant mà không so golden vì D3D11 và OpenGL làm tròn khác nhau.

## 5. Ghi chú orientation khi đọc pixel

Test integration dùng `gs_stage_texture()` + `gs_stagesurface_map()` để đọc pixel. Trên OpenGL/llvmpipe trong môi trường CI hiện tại, hàng đầu tiên map ra là **hàng trên** của ảnh (top-down), nên `render_pixel()` dùng `row = y` và pattern vẽ với gốc toạ độ top-based (`translate(x, y)`).

Nếu chạy trên backend trả về bottom-up (một số driver/backend có thể khác), các giá trị `pattern eye/mouth luma` sẽ lệch và test sẽ fail ở bước zone. Khi đó cần kiểm tra lại quy ước của backend thay vì sửa ngưỡng test.

## 6. Kiểm tra log OBS

Mở **Help → Log Files → View Current Log** và tìm:

- `Loading module: obs-beauty-filter` — module được load.
- `Failed to load '...beauty_filter.effect'` — **không** được xuất hiện.
- `Compiler warnings/errors for ...beauty_filter.effect` — **không** được xuất hiện.
- `Source ID 'beauty_filter' not found` — **không** được xuất hiện sau khi thêm filter.
- `[obs-beauty-filter] Failed to allocate GPU resources` — **không** được xuất
  hiện khi hoạt động bình thường. Khi có, filter hiện thông báo lỗi màu đỏ và
  cho nguồn đi qua nguyên vẹn.
- `[obs-beauty-filter] Could not read/write custom presets` — chỉ xuất hiện khi
  `custom-presets.json` không đọc được hoặc thư mục config không ghi được.
  Kiểm tra đường dẫn và quyền file; filter vẫn hoạt động nếu thiếu danh sách
  custom.

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
| 24 | Lưu custom preset, khởi động lại OBS | Preset hiện là `Custom: <tên>` và khôi phục đủ mọi control |
| 25 | Lưu đè tên đã có với hoa thường khác | Chỉ còn một mục, giá trị được cập nhật |
| 26 | Xóa custom preset đang chọn | Mục biến mất, lựa chọn quay về `Custom` |
| 27 | Reset toàn bộ | Mọi control trở về preset Default |
| 28 | Tạm đổi tên `data/beauty_filter.effect` | Filter cho nguồn đi qua, hiện thông báo lỗi GPU, không ra frame đen |
| 29 | Sửa `custom-presets.json` thiếu một khóa | Preset vẫn load, giá trị thiếu fallback về Default |
| 30 | Chạy bộ cài `.exe` trên máy OBS 33 sạch | Setup ghi DLL và `data\` vào `%ProgramData%\obs-studio\plugins\obs-beauty-filter`, OBS hiện **Beauty Filter**, gỡ cài chỉ xóa plugin |

## 8. Hướng dẫn benchmark — Intel i5-1235U

Đây là máy tham chiếu cho các ngưỡng hiệu năng: Intel Core i5-1235U (2 P-core
+ 8 E-core), Iris Xe 80 EU, 16 GB DDR4/DDR5 **dual-channel**, Windows 11,
OBS 30.2 trở lên. RAM single-channel gần như giảm một nửa băng thông iGPU và
không phải cấu hình so sánh hợp lệ.

### Cách đo

1. Dùng camera thật hoặc test source 30/60 fps ở độ phân giải cần đo. Không
   benchmark trên ảnh tĩnh hoặc frame bị pause.
2. Mở **Stats** của OBS (View → Stats) ít nhất 5 phút khi **chưa gắn filter**;
   ghi lại *average time to render frame*, *frames missed* và mức dùng GPU.
3. Gắn filter với preset và Quality cần đo, chạy thêm 5 phút và ghi cùng các
   chỉ số.
4. Filter overhead = frame time có filter − frame time không filter. Frame time
   tuyệt đối quan trọng hơn overhead vì OBS còn render scene và encode.
5. Xác nhận: không missed frame, GPU dưới 85–90 %, RAM không tăng, UI vẫn mượt.
6. Lặp lại cho từng preset thực dùng. Các preset khác nhau chủ yếu ở Radius,
   Detail, Glow và Quality; `Studio` và `Soft` nặng nhất.

Không dùng truy vấn `gs_timer` đồng bộ để đo production: nó làm nghẽn pipeline
GPU và làm kết quả cao hơn thực tế. Stats của OBS đã báo frame time mà không bị
nghẽn đó.

### Ngưỡng khuyến nghị

| Nguồn | FPS | Quality | Preset | Overhead tối đa | Frame time tổng tối đa | Điều kiện đạt |
|---|---:|---|---|---:|---:|---|
| 1080p | 30 | Balanced | Default / Natural | 3.0 ms | 20 ms | 0 missed frame trong 10 phút |
| 1080p | 60 | Balanced | Default / Natural | 4.0 ms | 12 ms | 0 missed frame, GPU < 90 % |
| 1080p | 60 | High | Cinema / Studio | 6.0 ms | 12 ms | 0 missed frame; hạ mức nếu drop |
| 1440p | 30 | High | Cinema | 6.0 ms | 20 ms | 0 missed frame |
| 1440p | 60 | Balanced | Cinema | 8.0 ms | 12 ms | 0 missed frame; High thường quá nặng |
| 4K | 30 | Balanced | Cinema | 12 ms | 25 ms | 0 missed frame; dùng Fast nếu cần |
| 4K | 60 | Fast | bất kỳ | 6.0 ms | 16 ms | Không khuyến nghị trên Iris Xe |

Đây là giá trị định hướng, không phải bảo đảm tuyệt đối: thermal limit, băng
thông bộ nhớ chia sẻ và các source khác trong OBS đều thay đổi kết quả. Máy
không đạt ngưỡng nên chuyển sang **Fast** trước khi giảm chất lượng hình ảnh
xuống dưới mức scene cần.

### Nếu benchmark không đạt

1. Hạ **Quality** một mức (High → Balanced → Fast).
2. Giảm **Radius** (ví dụ 10 → 6) và **Glow**.
3. Tắt **Face-aware zones** nếu không dùng teeth/eye.
4. Bỏ các filter xếp chồng (denoise/sharpen) hoặc chỉ chuyển chúng ra sau
   beauty filter nếu kết quả vẫn đúng.
5. Kiểm tra laptop đang cắm điện, chế độ hiệu năng cao, và camera không bị ép
   độ phân giải cao hơn canvas.

Ghi kết quả vào issue kèm preset, Quality, độ phân giải, frame time và phiên
bản OBS. Xem ma trận tương thích tại [COMPATIBILITY.md](COMPATIBILITY.md).

## 9. Kiểm tra hiệu năng

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

## 10. Kiểm tra chất lượng hình ảnh

- Chụp screenshot trước/sau với cùng ánh sáng, cùng frame.
- Kiểm tra ở 100% zoom các vùng: mắt, lông mày, viền môi, tóc mai, đường viền hàm.
- Kiểm tra da ở vùng sáng nhất và tối nhất để tránh mất chi tiết.
- Kiểm tra teeth/eye zones ở mặt nhìn thẳng, mặt quay 3/4 và khi có vật che.
- Với nhiều tông da, chụp ít nhất 3 mức sáng/tối khác nhau và xác nhận mask + face box.
- Với HDR, kiểm tra highlight (đèn, cửa sổ) trước/sau để chắc không bị clip.

## 11. Test trên Windows cụ thể

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

### Smoke test bộ cài

CI Windows biên dịch `installer/obs-beauty-filter.iss` bằng Inno Setup 6 sau
bước đóng gói, rồi cài silent vào thư mục tạm:

```powershell
.\obs-beauty-filter-1.5.0-windows-x64-setup.exe /VERYSILENT /SUPPRESSMSGBOXES `
  /NORESTART /SP- /DIR=C:\temp\beauty-installer-test /LOG=install.log
```

Test kiểm tra `obs-beauty-filter.dll`, `data\beauty_filter.effect`,
`data\locale\en-US.ini`, `data\locale\vi-VN.ini` và `unins000.exe`, chạy
uninstaller silent rồi xác nhận DLL đã biến mất.

Để thử trên máy local mà không đụng bản OBS thật, truyền `/DIR` trỏ tới thư mục
rỗng. Để thử nhận diện portable, dùng một cây OBS portable thật (hoặc cây tạm có
`bin\64bit\obs64.exe` và `portable_mode.txt`) rồi truyền `/OBSDIR=<cây-đó>`;
setup phải cài vào `<cây-đó>\plugins\obs-beauty-filter`. Chạy setup không kèm
tham số trên máy không có OBS sẽ cài vào `%ProgramData%\obs-studio\plugins`.

## 12. CI gợi ý

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
