# Ma trận tương thích

**[English](COMPATIBILITY.en.md) | Tiếng Việt**

Trang này liệt kê những tổ hợp đã được CI kiểm thử, đã kiểm thử thủ công và
những tổ hợp chỉ build được nhưng chưa kiểm thử. "CI" nghĩa là toàn bộ bộ test
tự động chạy trên mỗi push và tag.

## Tổ hợp đã kiểm thử

| Nền tảng | OBS Studio | Backend đồ họa | Trạng thái | Ghi chú |
|---|---|---|---|---|
| Linux x86_64, Alpine 3.21 | 30.2.3 | OpenGL / Mesa llvmpipe (phần mềm) | **CI, toàn bộ test** | Render matrix + golden image, load/render/HDR, ASan/UBSan |
| Windows x64 (GitHub `windows-2022`) | 33.0.1 (build từ source) | D3D11 (GPU của runner) | **CI, toàn bộ test** | Biên dịch effect, load/render, render-matrix invariants, đóng gói ZIP và smoke test bộ cài `.exe` |
| Windows 10/11 x64 | 33.0.1 | D3D11, GPU thật | Đã kiểm thử thủ công | Gói build sẵn, layout OBS 33+ |
| Windows 10/11 x64 | 30.2 – 32.x | D3D11, GPU thật | Build được / layout legacy | OBS 33 vẫn nạp layout legacy nhưng đánh dấu Legacy |
| Linux x86_64 | 30.2 – 33.x | OpenGL, GPU thật | Build được | Đường dẫn cài đặt tùy distro |
| macOS | 30.2+ | OpenGL (Apple đã deprecated) | Build được, chưa test | Không có gói build sẵn |

## Phiên bản OBS Studio

| OBS | Trạng thái | Layout plugin |
|---|---|---|
| 30.2 LTS | Hỗ trợ, CI | Legacy trên Windows (`obs-plugins/64bit` + `data/obs-plugins/...`) |
| 31.x | Hỗ trợ | Legacy trên Windows |
| 32.x | Hỗ trợ | Legacy trên Windows |
| 33.x | Hỗ trợ, CI | Layout mới (`plugins/obs-beauty-filter/...`) là mặc định |
| 34+ | Chưa phát hành | Dự kiến dùng layout mới; kiểm tra lại trước khi phát hành |

Plugin dùng API filter và graphics ổn định của `libobs`. Một bản OBS major mới
vẫn có thể thay đổi cách biên dịch shader, layout thư mục plugin hoặc thương
lượng không gian màu; hãy chạy lại bộ test và ma trận thủ công trước khi công
bố hỗ trợ.

## Yêu cầu build

| Yêu cầu | Tối thiểu |
|---|---|
| CMake | 3.28 |
| Trình biên dịch C | C99 (GCC, Clang hoặc MSVC 2022) |
| Bộ phát triển OBS | 30.2 (`libobsConfig.cmake` cho build standalone) |
| Runtime Linux | Driver OpenGL 3.3, X11 hoặc XWayland cho test headless |
| Runtime Windows | GPU hỗ trợ D3D11, OBS 30.2+ 64-bit |
| Sanitizer (tùy chọn) | Clang + compiler-rt trên musl/Alpine; GCC kèm libasan trên glibc |

## Hỗ trợ tính năng

| Tính năng | Trạng thái |
|---|---|
| SDR 8-bit (`GS_CS_SRGB`) | Hỗ trợ, đã test |
| SDR 16-bit float (`GS_CS_SRGB_16F`) | Hỗ trợ, có test render 16F |
| HDR Rec.709 extended (`GS_CS_709_EXTENDED`) | Hỗ trợ, đã test |
| HDR scRGB (`GS_CS_709_SCRGB`) | Hỗ trợ, đã test |
| Alpha | Được giữ nguyên, đã test |
| Dolby Vision / HDR transport khác | Không hỗ trợ |
| Nguồn 10-bit integer | OBS chuyển đổi trước filter; không phải đường xử lý native |
| Theo dõi nhiều khuôn mặt | Không hỗ trợ (chỉ một face box heuristic) |
| Compute shader | Không dùng; chạy trên OpenGL 3.3 / D3D11 feature level 10+ |

## Mức chất lượng

| Quality | Base pass | Phần cứng phù hợp |
|---|---|---|
| Fast | 1/4 độ phân giải | GPU tích hợp Intel/AMD, 4K, nhiều filter xếp chồng |
| Balanced | 1/3 độ phân giải | Mặc định cho 1080p60 |
| High | 1/2 độ phân giải | GPU rời, 1440p/4K, cận mặt |

## Khoảng trống đã biết

- Golden image được tạo trên Linux/llvmpipe. CI Windows chạy cùng các kiểm tra
  độ phân giải/alpha/16F nhưng không so pixel, vì D3D11 và OpenGL làm tròn
  khác nhau.
- Kiểm thử GPU thật (Intel Iris Xe, NVIDIA, AMD) là thủ công. Xem hướng dẫn
  benchmark trong [TESTING.md](TESTING.md).
- Đường D3D11 trên Windows được biên dịch và smoke-test trong CI; chất lượng
  hình ảnh trên camera thật vẫn cần đánh giá thủ công.
