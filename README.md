# OBS Beauty Filter

**[English](README.en.md) | Tiếng Việt**

[![Build](https://github.com/minhvuong097/obs-beauty-filter/actions/workflows/build.yml/badge.svg)](https://github.com/minhvuong097/obs-beauty-filter/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/minhvuong097/obs-beauty-filter?display_name=tag)](https://github.com/minhvuong097/obs-beauty-filter/releases/latest)

Plugin bộ lọc làm đẹp cho **OBS Studio** trên Windows và Linux (macOS có thể build từ source nhưng chưa được kiểm thử). Phiên bản **1.4.0** chạy hoàn toàn trên GPU, hòa vào pipeline filter tiêu chuẩn của OBS, không cần model AI, không đọc ngược frame full-resolution về CPU và không phụ thuộc runtime bên thứ ba.

Mục tiêu: làn da mịn tự nhiên cho livestream/webcam **và camera độ phân giải cao** mà vẫn giữ chi tiết mắt, lông mày, tóc, lỗ chân lông và viền khuôn mặt; đồng thời xử lý đúng nguồn HDR.

## Tính năng

- **Frequency separation cho da**: pass base bilateral ở 1/2–1/4 độ phân giải (25 mẫu Vogel disk), tái tạo chi tiết full-resolution — làm mịn mảng da lớn mà vẫn giữ texture và giảm halo ở 1440p/4K.
- **Làm mịn edge-preserving**: bilateral filter 9/13/25 mẫu, giảm mụn nhẹ và se khít lỗ chân lông nhưng không làm mất cạnh.
- **Skin mask nhiều tầng**: nhận diện tông da trong YCbCr, thêm **neighbourhood coherence**, **loại gradient/texture** (tóc, vân gỗ) và **temporal stabilisation** để giảm nhấp nháy.
- **Face region heuristic**: ước lượng khung mặt từ connected component của skin mask trên grid nhỏ 64×N, có temporal smoothing và **khung mặt thủ công** khi tự nhận diện sai. Không cần ONNX/MediaPipe.
- **Face prior**: giảm nhận nhầm tường/gỗ/quần áo màu da bằng ellipse mặt + cổ.
- **Teeth whitening**: làm trắng vùng răng trong ellipse miệng, chỉ trên pixel sáng, ít bão hòa và trung tính.
- **Eye brightening**: làm sáng lòng trắng và nâng nhẹ iris/pupil trong hai ellipse mắt.
- **Giữ chi tiết**: tách high-frequency, ưu tiên giữ luma (lỗ chân lông) hơn chroma (mảng đỏ), làm nét vùng không phải da.
- **Tinh chỉnh sắc tố da**: sáng kiểu phơi sáng, ấm giữ luma, bão hòa, sắc hồng — cân theo skin mask.
- **Soft glow**: ánh sáng mềm chỉ trên highlight, vùng tối vẫn sạch.
- **Chống dải màu**: triangular dithering cho ngõ ra 8-bit, tránh banding khi da mịn trên nền gradient.
- **HDR-safe**: xử lý `GS_CS_709_EXTENDED`, `GS_CS_709_SCRGB` và `GS_CS_SRGB_16F` trong linear light, giữ highlight > mức trắng SDR (không bypass, không clip).
- **Preview**: skin mask, split view và face/eye/mouth zones.
- **Preset khởi đầu**: Natural, Cinema, Studio, Soft, Beard, Dark skin, HDR và Default. Chọn preset rồi tinh chỉnh; mọi thay đổi tay sẽ tự chuyển về **Tùy chỉnh**.
- **Preset tùy chỉnh**: lưu, ghi đè, tải lại và xóa phong cách của bạn. Dữ liệu nằm trong `custom-presets.json` ở thư mục cấu hình module OBS, tồn tại qua thay đổi scene collection và khởi động lại.
- **Nút Reset**: đưa toàn bộ thông số về preset Default mà không cần xóa và thêm lại filter.
- **Lỗi an toàn**: nếu effect hoặc texture GPU không tạo được, trang thuộc tính hiện thông báo lỗi và nguồn đi qua nguyên vẹn thay vì ra frame đen.
- **3 mức hiệu năng**: Fast / Balanced / High.
- **Hỗ trợ alpha**, SDR 8-bit và SDR 16-bit.
- **Song ngữ**: English và Tiếng Việt.

## Cấu trúc project

```
obs-beauty-filter/
├── CHANGELOG.md
├── CMakeLists.txt
├── src/
│   ├── beauty-filter.c      # OBS filter + properties + multi-pass render
│   ├── beauty-filter.h
│   ├── beauty-custom-presets.c # lưu preset người dùng dạng JSON
│   ├── beauty-custom-presets.h
│   ├── beauty-presets.c     # preset khởi đầu có sẵn
│   ├── beauty-presets.h
│   ├── beauty-settings.h    # tên setting obs_data dùng chung
│   ├── face-region.c        # heuristic face-box estimation (CPU, tiny grid)
│   ├── face-region.h
│   └── plugin-main.c        # module entry point
├── data/
│   ├── beauty_filter.effect # OBS effect: MaskPass / MaskAnalysis / BlurBase / Draw*
│   └── locale/
│       ├── en-US.ini
│       └── vi-VN.ini
├── tests/
│   ├── load-test.c          # load + render + mask/face/HDR integration test
│   ├── render-matrix-test.c # 8 preset x 4 độ phân giải + golden 16F
│   ├── preset-test.c        # unit test preset có sẵn và tùy chỉnh
│   ├── face-region-test.c   # unit test cho face detector
│   └── golden/
│       └── render-matrix.txt # golden image dạng lấy mẫu cho CI Linux
├── docs/
│   ├── ARCHITECTURE.md      # + ARCHITECTURE.en.md
│   ├── TUNING.md            # + TUNING.en.md
│   ├── TESTING.md           # + TESTING.en.md
│   └── COMPATIBILITY.md     # + COMPATIBILITY.en.md
└── scripts/
    ├── build-linux.sh
    └── build-windows.ps1
```

## Cài đặt trên Windows

### Cách A — dùng bản build sẵn

Tải bản mới nhất tại **[GitHub Releases](https://github.com/minhvuong097/obs-beauty-filter/releases/latest)**:

- Windows x64: `obs-beauty-filter-1.4.0-windows-x64.zip`
- Linux x86_64: `obs-beauty-filter-linux-x86_64.tar.gz`
- Mỗi release có kèm `SHA256SUMS.txt` để kiểm tra tệp tải về.

**OBS Studio 33+ — layout mới (khuyến nghị):**

1. Đóng OBS Studio.
2. Tạo thư mục:
   `C:\ProgramData\obs-studio\plugins\obs-beauty-filter\`
3. Copy `obs-beauty-filter.dll` vào thư mục đó.
4. Copy nguyên thư mục `data` vào:
   `C:\ProgramData\obs-studio\plugins\obs-beauty-filter\data\`
   Kết quả phải có:
   - `...\obs-beauty-filter\obs-beauty-filter.dll`
   - `...\obs-beauty-filter\data\beauty_filter.effect`
   - `...\obs-beauty-filter\data\locale\vi-VN.ini`
5. Mở OBS → chọn source camera → **Filters** → dấu **+** → **Beauty Filter**.

> `%ProgramData%` thường là thư mục ẩn. Với OBS portable, dùng
> `<thư-mục-OBS-portable>\plugins\obs-beauty-filter\` thay cho
> `C:\ProgramData\obs-studio\plugins\obs-beauty-filter\`.

**OBS 32 trở xuống — layout legacy:**

1. Copy `obs-beauty-filter.dll` vào:
   `C:\Program Files\obs-studio\obs-plugins\64bit\`
2. Copy nội dung thư mục `data` vào:
   `C:\Program Files\obs-studio\data\obs-plugins\obs-beauty-filter\`

> OBS 33 vẫn load layout legacy nhưng sẽ gắn nhãn **Legacy** trong Plugin
> Manager; OBS 34 sẽ ngừng load layout này. Vì vậy nên dùng layout mới ở trên.
>
> Nếu OBS 33 khởi động ở **Safe Mode**, plugin bên thứ ba sẽ không được load.
> Hãy thoát Safe Mode rồi mở lại OBS.

### Cách B — build từ source (khuyến nghị khi phát triển)

**Build in-tree** (đơn giản nhất nếu đã build OBS từ source):

```powershell
# Copy thư mục này vào obs-studio/plugins/obs-beauty-filter
# Thêm dòng sau vào obs-studio/plugins/CMakeLists.txt:
#   add_core_module(obs-beauty-filter)
```

Sau đó build OBS như bình thường (CMake preset + Visual Studio 2026 hoặc
toolchain mà bản OBS 33 của bạn yêu cầu). Plugin và data sẽ được copy vào thư
mục build của OBS theo layout core-module của OBS. Cách này chỉ dành cho môi
trường phát triển: trên OBS 33, thư mục `core` chỉ dành cho plugin chính thức.
Bản phát hành cho người dùng nên build standalone để cài vào thư mục `plugins`.

**Build standalone** (cần OBS SDK/build tree có `libobsConfig.cmake`):

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_PREFIX_PATH="C:\obs-studio\build_x64;C:\obs-deps\x64"
cmake --build build --config RelWithDebInfo
cmake --install build --config RelWithDebInfo `
  --prefix "$env:ProgramData\obs-studio\plugins"
```

Nếu dùng Visual Studio 2022, thay generator bằng
`"Visual Studio 17 2022"`. Với layout mới, `--prefix` phải trỏ tới **thư mục
plugins**, không phải thư mục cài OBS:

- OBS thường: `%ProgramData%\obs-studio\plugins`
- OBS portable: `<thư-mục-OBS-portable>\plugins`

Muốn đóng gói theo layout cũ cho OBS 32 trở xuống, thêm
`-DBEAUTY_OBS33_PLUGIN_LAYOUT=OFF` và dùng prefix `C:\Program Files\obs-studio`.

Hoặc dùng script:

```powershell
# Build + cài vào layout OBS 33+
.\scripts\build-windows.ps1 -ObsBuild "C:\obs-studio\build_x64" `
  -ObsSdk "C:\obs-deps\x64" `
  -InstallPrefix "$env:ProgramData\obs-studio\plugins"

# Build + tạo zip phân phối (root zip là thư mục obs-beauty-filter\)
.\scripts\build-windows.ps1 -ObsBuild "C:\obs-studio\build_x64" `
  -ObsSdk "C:\obs-deps\x64" -Package

# Layout legacy cho OBS 32 trở xuống
.\scripts\build-windows.ps1 -Layout legacy `
  -InstallPrefix "C:\Program Files\obs-studio"
```

> Script tự chọn generator Visual Studio 2026 khi cả CMake và VS 2026 đều có
> trên máy, ngược lại dùng Visual Studio 2022. File zip tạo trong `dist\`; giải
> nén vào đúng thư mục `plugins` ở trên (hoặc `...\plugins\` của bản portable).

> Nếu chỉ có OBS đã cài mà không có OBS SDK/source, hãy dùng bản build sẵn ở **Cách A**. Muốn tự build thì cần OBS source (in-tree) hoặc OBS SDK/CMake package (standalone).

### Linux

```bash
sudo apt install cmake build-essential libobs-dev   # hoặc package tương đương
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

- **Debian/Ubuntu:** thêm `-DCMAKE_INSTALL_LIBDIR="lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)"`
  để plugin vào đúng `/usr/lib/<triplet>/obs-plugins`.
- **Fedora/Arch:** kiểm tra thư mục `obs-plugins` của OBS và đặt `CMAKE_INSTALL_LIBDIR`
  tương ứng nếu cần.
- Không nên dùng prefix mặc định `/usr/local` trừ khi OBS của bạn có tìm kiếm đường dẫn đó.

Hoặc `./scripts/build-linux.sh` rồi `sudo cmake --install build` (mặc định prefix `/usr`;
đặt `INSTALL_LIBDIR=...` nếu dùng Debian/Ubuntu multiarch).

## Sử dụng nhanh

1. Thêm **Beauty Filter** vào source camera (hoặc scene chứa camera).
2. Chọn **Preset khởi đầu** phù hợp camera (`Natural` cho webcam 1080p, `Cinema` cho 1440p/4K, `Studio` cho cận mặt), hoặc bắt đầu với preset mặc định:
   - **Amount** `0.35`
   - **Radius** `6`
   - **Detail preservation** `0.35`
   - **Skin detection** bật, **Skin sensitivity** `0.50`
   - **Face-aware zones** bật, **Temporal mask stability** `0.60`
   - **Rosy tint** `0.15`
   - **Teeth whitening / Eye brightening** `0` (bật khi cần)
3. Khi đã vừa ý, nhập tên vào **Tên preset tùy chỉnh** và bấm **Lưu thông số hiện tại thành preset**; preset xuất hiện trong menu dưới dạng `Tùy chỉnh: <tên>`. **Đặt lại toàn bộ về mặc định** đưa mọi thông số về Default.
4. Bật **Show skin mask** để kiểm tra mask da. Nếu tường/gỗ màu da cũng trắng, giữ **Face-aware zones** bật và giảm **Skin sensitivity**.
5. Bật **Show face zones** để kiểm tra khung mặt, hai mắt và miệng. Nếu khung sai, bật **Manual face box** rồi chỉnh `Face center X/Y`, `Face width/height`.
6. Tăng **Teeth whitening** `0.2–0.5` và **Eye brightening** `0.1–0.3`. Nếu vùng da quanh mắt/miệng bị sáng theo, giảm hai thông số này.
7. Bật **Split preview** để so sánh nửa trái (gốc) và nửa phải (đã xử lý), sau đó tắt.
8. Nếu da bị bệt: tăng **Detail preservation**, giảm **Amount** hoặc **Radius**.
9. Nếu mask nhấp nháy khi đầu chuyển động: tăng **Temporal mask stability** (`0.6–0.75`). Nếu bị bóng mờ (ghosting), giảm xuống `0.3–0.5`.

## Thông số

| Thông số | Mặc định | Khoảng | Ý nghĩa |
|---|---:|---:|---|
| Amount | 0.35 | 0–1 | Mức làm mịn da |
| Radius (px @1080p) | 6 | 0–24 | Bán kính lấy mẫu, tự scale theo chiều cao frame |
| Detail preservation | 0.35 | 0–1 | Giữ lại texture/chi tiết da |
| Skin detection | On | On/Off | Chỉ xử lý vùng da |
| Skin sensitivity | 0.50 | 0–1 | Mở rộng/thu hẹp mask da |
| Show skin mask | Off | On/Off | Xem mask trắng/đen để tinh chỉnh |
| Brightness | 0 | -50..50 | Sáng da (theo mask) |
| Warmth | 0 | -50..50 | Ấm/ngả vàng da |
| Saturation | 0 | -50..50 | Bão hòa màu da |
| Rosy tint | 0.15 | 0–1 | Sắc hồng tự nhiên |
| Clarity | 0.10 | 0–1 | Làm nét vùng không phải da |
| Soft glow | 0.05 | 0–1 | Ánh sáng mềm |
| Face-aware zones | On | On/Off | Bật face prior + teeth/eye zones |
| Temporal mask stability | 0.60 | 0–1 | Trộn mask với history; cao = ổn định hơn, ít nhấp nháy hơn |
| Teeth whitening | 0 | 0–1 | Làm trắng răng trong vùng miệng |
| Eye brightening | 0 | 0–1 | Làm sáng mắt trong vùng mắt |
| Show face zones | Off | On/Off | Hiện face prior (xanh), mắt (xanh dương), miệng (vàng) |
| Manual face box | Off | On/Off | Dùng khung mặt thủ công thay vì auto |
| Face center X / Y | 0.50 / 0.45 | 0–1 | Tâm mặt, tọa độ chuẩn hóa |
| Face width / height | 0.32 / 0.42 | 0.05–1 | Kích thước **đầy đủ** của khung mặt, chuẩn hóa |
| Quality | Balanced | Fast/Balanced/High | Base 1/4, 1/3 hoặc 1/2 độ phân giải; High khuyến nghị cho 1440p/4K |
| Anti-banding | On | On/Off | Triangular dithering cho ngõ ra 8-bit |
| Split preview | Off | On/Off | Nửa trái gốc, nửa phải đã xử lý |
| Overall opacity | 1.0 | 0–1 | Trộn tổng với ảnh gốc |

Ghi chú: `Face width/height` là kích thước đầy đủ (full width/height), không phải bán kính. Ví dụ mặt chiếm 32% chiều rộng và 42% chiều cao khung hình.

### Gợi ý theo tình huống

- **Webcam 1080p, đèn trong nhà**: Amount `0.35–0.5`, Radius `5–8`, Detail `0.3–0.5`, Clarity `0.05–0.15`.
- **Camera cao cấp 1440p/4K**: Quality `High`, Amount `0.20–0.35`, Radius `6–12`, Detail `0.45–0.65`, Clarity `0.05–0.10`, Skin sensitivity `0.45–0.55`. Base pass giữ chi tiết full-resolution nên da mịn nhưng không bệt.
- **Close-up / da có khuyết điểm**: Amount `0.55–0.7`, Radius `8–12`, Detail `0.25`, Quality `High`, Teeth `0.3–0.5`, Eyes `0.2–0.4`.
- **Nam giới / có râu**: Amount `0.2–0.3`, Detail `0.5–0.7`, Radius `4–6`, Clarity `0.1`; giảm Teeth/Eyes.
- **Da sẫm màu**: Sensitivity `0.6–0.75`, Amount `0.25–0.4`; luôn kiểm tra bằng **Show skin mask**.
- **Thiếu sáng / noise**: Beauty Filter không khử noise. Hãy thêm **Denoise** trước, rồi Amount `0.2–0.35`, Glow `0.1`.
- **4K / nhiều filter xếp chồng**: Quality `High` nếu GPU rời còn dư; nếu tụt frame chuyển `Balanced` (base 1/3) trước khi hạ `Fast`. Radius tự động scale theo độ phân giải.
- **Nguồn HDR (Rec.709 extended / scRGB / 16F)**: dùng cùng thông số; filter xử lý trong linear và giữ highlight. Kiểm tra lại Amount/Glow vì HDR có dải sáng rộng hơn.

Xem thêm [docs/TUNING.md](docs/TUNING.md).

## Kiến trúc kỹ thuật

```
Camera frame (SDR 8-bit / SDR 16F / Rec.709 extended / scRGB)
   │
   ▼
[Custom capture]
   │  gs_texrender_begin_with_color_space(output_space)
   ▼
Source texture
   │
   ├─► MaskPass (half-res RGBA8, ping-pong)
   │      ├─ YCbCr skin probability
   │      ├─ neighbourhood coherence (8 mẫu)
   │      ├─ gradient/texture guard
   │      ├─ temporal blend với history
   │      └─ face prior (ellipse mặt + cổ)
   │      Output: R = raw mask, G = stabilized mask
   │
   ├─► MaskAnalysis (64×N)  ──► CPU readback nhỏ (mỗi 6 frame)
   │                              └─► face-region.c: connected component
   │                                  + upper-band → face box
   │
   ├─► BlurBase (1/2–1/4 res, RGBA16F)
   │      └─ 25-tap Vogel disk bilateral → base tần số thấp
   │
   └─► DrawFast / DrawBalanced / DrawHigh
          ├─ frequency separation: base + detail tái tạo
          ├─ detail reconstruction + clarity
          ├─ complexion + glow (HDR-safe)
          ├─ teeth/eye zones (chỉ khi có face box)
          ├─ anti-banding dither
          ├─ opacity / split / previews
          └─ HDR scale restore
   │
   ▼
Output (giữ alpha, giữ giá trị > SDR white trên HDR)
```

- **Không còn một pass duy nhất**: mask pass chạy ở nửa resolution, analysis pass ở 64×N, base pass ở 1/2–1/4 resolution, main pass ở full resolution.
- **Frequency separation**: base pass làm mờ mảng da lớn ở độ phân giải thấp với 25 mẫu Vogel disk; pass cuối lấy lại chi tiết full-resolution, ưu tiên luma hơn chroma. Cách này giữ lỗ chân lông, giảm halo và tránh kernel quá thưa ở 1440p/4K.
- **Không readback full frame**: chỉ đọc grid 64×N (tối đa 64×64 RGBA = 16 KB) và chỉ khi `Face-aware zones` bật, không dùng manual box, mỗi 6 frame.
- **Ping-pong mask texture** cung cấp history cho temporal stabilisation mà không cấp phát mỗi frame.
- **Base texture RGBA16F** ngay cả với SDR để tránh lượng tử hoá linear 8-bit trong vùng tối trước khi encode sRGB.
- Bán kính được chuẩn hóa theo chiều cao frame (1080p là mốc), giúp cùng một bộ thông số cho kết quả tương đương ở 720p/1080p/4K.
- **HDR**: `GS_CS_709_EXTENDED` và `GS_CS_SRGB_16F` dùng `hdr_scale = 1.0`; `GS_CS_709_SCRGB` dùng `80 / obs_get_video_sdr_white_level()`. Frame được capture trực tiếp vào **output space** để OBS tự chuyển đổi/tonemap nguồn; shader quy đổi sample về linear tương đối SDR đúng một lần, không `saturate` output HDR và chia lại scale trước khi ghi ra.
- Chi tiết toán học và quyết định thiết kế: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Hiệu năng

Chi phí chính là fragment shader full-resolution, cộng thêm mask pass nửa resolution, base pass 1/2–1/4 resolution (25 mẫu) và readback nhỏ:

| Quality | Base | Ghi chú |
|---|---|---|
| Fast | 1/4 res | iGPU, 4K, hoặc nhiều filter xếp chồng |
| Balanced | 1/3 res | Mặc định cho 1080p60 |
| High | 1/2 res | Close-up, GPU rời, 1440p/4K — khuyến nghị cho camera cao cấp |

- `MaskPass` chạy ở nửa resolution với 8 mẫu coherence + 4 mẫu gradient; rẻ hơn nhiều so với full-res.
- `MaskAnalysis` chỉ chạy mỗi 6 frame khi face-aware auto bật.
- `BlurBase` luôn rẻ hơn một bilateral full-res cùng bán kính vì chỉ chạy trên 1/4–1/16 số pixel.
- Tắt **Face-aware zones** nếu không dùng teeth/eye để bỏ mask pass khi `Skin detection` cũng tắt.
- Nếu tụt frame: hạ Quality trước, sau đó giảm Radius, tắt Glow.

## Giới hạn đã biết

- **Face detection là heuristic**, không phải ML landmark/segmentation. Detector tìm component da lớn nhất và ước lượng mặt từ dải trên; có thể sai khi có nhiều người, mặt bị che, đội mũ, ánh sáng mạnh hoặc nền cùng màu da. Dùng **Show face zones** và **Manual face box** để xử lý.
- **Teeth/eye zones là ellipse ước lượng**, không bám theo landmark từng răng/mí mắt. Kết quả tốt nhất khi mặt nhìn thẳng, chiếm phần lớn khung hình.
- Skin mask vẫn dựa trên màu; coherence + face prior giảm nhưng không loại bỏ hoàn toàn gỗ/tường/quần áo màu da.
- Không khử noise.
- HDR được xử lý nhưng không tone-map; filter giữ nguyên dải động của nguồn.
- Kết quả phụ thuộc tông da và điều kiện ánh sáng; nên test với nhiều tông da trước khi dùng cho production.
- Đường D3D11 trên Windows cần được build/test trực tiếp trên máy Windows trước khi phát hành production.

## Roadmap

1. Compute shader (D3D11/OpenGL) cho radius lớn ở 4K.
2. Tuỳ chọn ML landmark/segmentation nhẹ (ONNX/MediaPipe) cho multi-face và occlusion.
3. Tự động chọn face box bằng landmark thay vì upper-band heuristic.
4. Chẩn đoán thời gian GPU tuỳ chọn, tránh kiểu đọc timer đồng bộ của `gs_timer`.

## Kiểm thử

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBEAUTY_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"

# Unit test cho face detector, preset và render matrix
./build/beauty-face-region-test
./build/beauty-preset-test
xvfb-run -a ./build/beauty-render-matrix-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter

# Load + render + mask/face/HDR integration test
xvfb-run -a ./build/beauty-load-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter \
  /usr/lib/obs-plugins/image-source.so \
  /usr/share/obs/obs-plugins/image-source

# Toàn bộ suite dưới ASan/UBSan (Clang trên musl/Alpine)
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DBEAUTY_BUILD_TESTS=ON -DBEAUTY_SANITIZE=address,undefined
cmake --build build-sanitize -j"$(nproc)"
```

Kỳ vọng các dòng cuối:

```text
OK: built-in and custom presets, lookup, apply, defaults and NULL handling verified
OK: render matrix verified 4 resolutions x 8 presets, opaque alpha, 16F range and finite output
OK: module loaded, filter created, effect compiled, filter rendered, mask/opacity/face zones/texture/HDR verified
```

Test integration kiểm tra: filter mặc định, skin mask, opacity 0, teeth/eye zones (có mặt thủ công), zone preview, **frequency separation trên pattern da có texture** (contrast giảm khi Detail thấp và quay lại khi Detail cao), HDR Rec.709 extended và scRGB (giữ giá trị > 1, thực sự được xử lý và không bị scale hai lần), và các control preset/lưu/xóa/reset có tồn tại. Render matrix chạy cả 8 preset ở 360p, 720p, 1080p và 1440p (4K tùy chọn), kiểm tra alpha đục, ngõ ra 16F hữu hạn và so golden image dạng lấy mẫu trên Linux. Không được có lỗi shader trong log. Xem [docs/TESTING.md](docs/TESTING.md) cho test matrix đầy đủ và hướng dẫn benchmark i5-1235U.

> **Trạng thái kiểm thử:** GitHub Actions build và chạy test tự động trên hai nền tảng. Linux (Alpine 3.21, OBS 30.2.3, OpenGL/Mesa llvmpipe) chạy đầy đủ `beauty-face-region-test`, `beauty-preset-test`, `beauty-render-matrix-test` (kèm golden image) và `beauty-load-test` (render, mask, texture, HDR), sau đó lặp lại dưới ASan/UBSan. Windows x64 build trực tiếp với source OBS 33.0.1 + `obs-deps`, chạy cùng bộ test trên đường D3D11 thật (render matrix invariant, load module + compile effect), rồi đóng gói theo layout plugin mới. Vẫn nên xác nhận render/HDR thủ công trong OBS trên phần cứng thật; xem [docs/TESTING.md](docs/TESTING.md) và [ma trận tương thích](docs/COMPATIBILITY.md).

## Changelog và tương thích

Xem [CHANGELOG.md](CHANGELOG.md) cho ghi chú phát hành và
[docs/COMPATIBILITY.md](docs/COMPATIBILITY.md) cho ma trận OBS, nền tảng và
không gian màu đã kiểm thử.

## Giấy phép

GPL-2.0-or-later. Xem [LICENSE](LICENSE).

Đây là project độc lập, không thuộc OBS Project. OBS Studio là phần mềm của OBS Project và các contributor.
