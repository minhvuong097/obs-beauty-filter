# Kiến trúc OBS Beauty Filter

**[English](ARCHITECTURE.en.md) | Tiếng Việt**

Tài liệu này mô tả luồng xử lý, công thức shader và các quyết định thiết kế của plugin **1.3.0** để ổn định trong pipeline filter của OBS.

## 1. Vòng đời filter và pipeline render

Plugin đăng ký một `obs_source_info`:

```c
.id = "beauty_filter"
.type = OBS_SOURCE_TYPE_FILTER
.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB
```

Mỗi frame, `beauty_render()` thực hiện:

1. Lấy target/parent bằng `obs_filter_get_target()` / `obs_filter_get_parent()`.
2. Lấy kích thước và color space ưu tiên:

   ```c
   GS_CS_SRGB, GS_CS_SRGB_16F, GS_CS_709_EXTENDED, GS_CS_709_SCRGB
   ```

   Color space khác (ví dụ HLG/PQ không được map) sẽ được `obs_source_skip_video_filter()`.
3. Cấp phát (lazy) `src_texrender`, hai `mask_texrender`, `analysis_texrender`, `base_texrender`, `analysis_stage` và `white_tex` nếu kích thước, color space hoặc quality thay đổi.
4. **Capture**: render target vào `src_texrender` bằng
   `gs_texrender_begin_with_color_space(..., gs_get_color_space())`. Dùng **output space** để OBS tự chuyển đổi/tonemap nguồn (kể cả scRGB) và tránh scale hai lần; dùng `obs_source_default_render()` cho input thông thường, hoặc `obs_source_video_render()` cho custom/async source.
5. **MaskPass** (nếu cần mask): render mask nửa resolution vào `mask_texrender[mask_index]`, lấy history từ texture còn lại.
6. **MaskAnalysis** (định kỳ): downsample mask xuống grid 64×N và readback nhỏ để cập nhật face box.
7. **BlurBase**: render base bilateral độ phân giải thấp vào `base_texrender` (1/2–1/4 độ phân giải, RGBA16F).
8. **Main pass**: chọn `DrawFast` / `DrawBalanced` / `DrawHigh`, set toàn bộ uniform rồi vẽ fullscreen quad, tái tạo chi tiết full-resolution trên base.
9. Khôi phục blend state, framebuffer sRGB và linear sRGB.

Không dùng `obs_source_process_filter_begin_with_color_space()` vì plugin cần kiểm soát texture capture, mask pass và color space riêng. Việc capture thủ công vẫn tương thích với filter chain vì target/parent được lấy đúng từ OBS.

Effect được tạo một lần trong `create()` bằng `gs_effect_create_from_file()` và hủy trong `destroy()`. Không có effect nào được tạo trong `render()`.

## 2. Skin mask pass

`MaskPass` chạy ở **nửa resolution** (`(width+1)/2 × (height+1)/2`), định dạng RGBA8:

- `R` = raw mask (dùng cho face detector, không phụ thuộc face prior).
- `G` = stabilized mask (dùng cho main pass).
- `B` = 0, `A` = 1.

### 2.1. YCbCr

```hlsl
Y  = 0.299R + 0.587G + 0.114B
Cb = -0.168736R - 0.331264G + 0.5B + 0.5
Cr =  0.5R - 0.418688G - 0.081312B + 0.5
```

Da người rơi quanh một ellipse chroma. Tâm ellipse **dịch theo luma** vì da sẫm màu có chroma lệch về phía Cb cao/Cr thấp hơn da sáng:

- da sẫm: tâm `(Cb 0.455, Cr 0.555)`, bán kính `(0.060, 0.065)`
- da sáng: tâm `(Cb 0.385, Cr 0.600)`, bán kính `(0.100, 0.090)`
- nội suy bằng `smoothstep(0.05, 0.55, Y)`

```hlsl
float t = smoothstep(0.05, 0.55, Y);
float2 center = lerp(float2(0.455, 0.555), float2(0.385, 0.600), t);
float2 radii  = lerp(float2(0.060, 0.065), float2(0.100, 0.090), t);
float2 d = (float2(cb, cr) - center) / radii;
float chroma = 1.0 - smoothstep(0.55, 1.30, length(d));
```

Mask được tính trên giá trị **sRGB** (`linear_to_srgb(...)`) vì shader chạy với `OBS_SOURCE_SRGB` và nhận sample ở linear-light. Các phép tone/glow vẫn thực hiện trên linear-light.

### 2.2. Luma gate và red dominance

```hlsl
luma_gate = smoothstep(0.015, 0.080, Y) * (1 - smoothstep(0.95, 1.0, Y));
red_dom   = smoothstep(-0.020, 0.060, R-B) * smoothstep(-0.035, 0.035, R-G);
```

- `luma_gate` loại pixel quá tối (tóc, mắt, nền đen) và highlight cháy.
- `red_dom` loại nền xanh/lục và giảm khả năng nhận nhầm môi đỏ đậm.

### 2.3. Sensitivity

```hlsl
p = saturate(chroma * luma_gate * red_dom);
mask = pow(p, lerp(1.70, 0.55, sensitivity));
```

- `sensitivity = 0`: mask chặt, ít nhận nhầm.
- `sensitivity = 1`: mask rộng, bao phủ tốt hơn nhưng dễ bắt cả gỗ/tường màu da.

### 2.4. Neighbourhood coherence

Mask ban đầu được kiểm tra với 8 mẫu quanh pixel (khoảng cách `1.5 texel`, mẫu chéo nhân `0.7071`):

```hlsl
support = average(skin_probability(uv + offset));
coherence = smoothstep(0.10, 0.45, support);
raw = center_p * lerp(0.55, 1.0, coherence) * texture_guard;
```

- Pixel da đơn lẻ (nhiễu, điểm ảnh lạc) bị giảm mạnh.
- Vùng da liên tục giữ gần như đầy đủ.
- Đây là bước quan trọng để loại các đốm màu da nhỏ trên nền.

### 2.5. Gradient/texture guard

Lấy luma của 4 pixel lân cận để tính gradient:

```hlsl
gx = Y(right) - Y(left);
gy = Y(up)    - Y(down);
gradient = sqrt(gx*gx + gy*gy);
texture_guard = 1.0 - 0.85 * smoothstep(0.035, 0.16, gradient);
```

Vân gỗ, sợi tóc, râu và cạnh vật thể có gradient cao nên mask bị giảm; da phẳng gần như không bị ảnh hưởng.

### 2.6. Temporal stabilisation

`MaskPass` đọc stabilized mask của frame trước từ `history_tex.g` (ping-pong giữa hai `mask_texrender`):

```hlsl
previous = history_tex.g;
motion   = smoothstep(0.10, 0.45, abs(raw - previous));
keep     = temporal_strength * (1.0 - motion) * history_valid;
stabilized = lerp(raw, previous, keep);
```

- Vùng đứng yên giữ history → giảm nhấp nháy.
- Vùng thay đổi mạnh (đầu quay, cắt cảnh) tin vào frame hiện tại → tránh bóng mờ.
- `history_valid = 0` ở frame đầu hoặc sau khi đổi kích thước/color space.
- `temporal_strength = 0` tắt hoàn toàn temporal.

### 2.7. Face prior

Khi có face box hợp lệ, stabilized mask được nhân với một ellipse mềm bao mặt + cổ:

```hlsl
center = face_center + float2(0.0, face_size.y * 0.22);
radii  = max(face_size * float2(1.15, 1.55), 0.0001);
prior  = 1.0 - smoothstep(0.85, 1.20, length((uv - center) / radii));
stabilized *= lerp(1.0, prior, 0.90 * face_valid * face_aware);
```

Face prior **chỉ áp lên stabilized mask**, không áp lên raw mask. Nhờ vậy detector face (đọc raw mask) không phụ thuộc vào output của chính nó và không bị "tự khoá" vào một khung sai.

## 3. Face region heuristic

### 3.1. Vì sao không dùng ML?

Mục tiêu là plugin nhẹ, không model, không dependency. Một detector heuristic đủ tốt cho webcam một người, và luôn có **manual face box** làm fallback. ML landmark/segmentation được để ở roadmap.

### 3.2. Grid và readback

- `MaskAnalysis` downsample raw mask xuống grid **64 × N** (N tỉ lệ theo aspect, kẹp 16..64) bằng 4×4 box filter (16 tap).
- Grid được stage và map xuống CPU. Kích thước tối đa 64×64 RGBA = 16 KB.
- Chỉ chạy khi `face_aware` bật, `face_manual` tắt, và `frame_counter % 6 == 0` → khoảng 5 lần/giây ở 30 fps.
- Không bao giờ đọc full-resolution frame.

### 3.3. Connected component và face box

`face-region.c`:

1. Duyệt grid, flood-fill 4 láng giềng các pixel `R >= 64` thành component.
2. Bỏ component có diện tích < 8.
3. Với mỗi component, lấy dải trên từ **6% đến 34%** chiều cao component (trán + má trên).
4. Dải phải có:
   - chiều rộng 4%..80% chiều rộng frame,
   - mật độ pixel da ≥ 22%,
   - ít nhất 6 pixel,
   - tâm ngang trong 8%..92% frame.
5. Ước lượng:

   ```text
   face_w = band_w * 1.10
   face_h = face_w * 1.35          (kẹp ≤ 98% chiều cao frame, ≥ 10%)
   face_cx = tâm ngang của band
   face_cy = top của component + face_h / 2
   half_w = face_w * 0.5 * 1.12
   half_h = face_h * 0.5 * 1.12
   ```

6. Chọn component có score cao nhất:

   ```text
   score = area
         * smoothstep(0.22, 0.55, density)
         * (0.55 + 0.45 * smoothstep(0.12, 0.45, band_w / component_h))
         * (1 - 0.70 * |face_cx - 0.5|)
         * (1 - 0.35 * face_cy)
   ```

   Score ưu tiên component lớn, dày, có tỉ lệ hợp lý và gần trung tâm phía trên khung hình.

### 3.4. Temporal smoothing và hold

- Khi detect thành công, face box được làm mượt với box trước:

  ```c
  box += (new_box - box) * 0.35f;
  ```

- Khi detect thất bại, giữ box cũ tối đa **5 lần cập nhật** rồi mới vô hiệu hoá. Điều này tránh mất teeth/eye zones khi mặt quay nhẹ hoặc bị che tạm thời.
- Khi `face_manual` bật, auto detection không chạy; box thủ công (`face_cx`, `face_cy`, `face_w`, `face_h`) được dùng trực tiếp. `face_w/h` là kích thước đầy đủ, chuẩn hóa.

### 3.5. Giới hạn

Detector không phân biệt mặt với cánh tay/cổ áo cùng màu da, không xử lý nhiều mặt, và có thể chọn nhầm component lớn hơn (ví dụ tường). Vì vậy:

- Face prior chỉ nhân mềm (`90%`), không cắt cứng.
- Teeth/eye chỉ hoạt động khi có box hợp lệ.
- Manual box là fallback chính thức.

## 4. Teeth whitening và eye brightening

Hai vùng được suy ra từ face box (half-size `fh = face_size`):

```hlsl
face_ok = saturate(face_aware * face_valid);

eye_y   = face_center.y - 0.16 * fh.y;
eye_dx  = 0.40 * fh.x;
eye_l   = ellipse(uv, center - (eye_dx, 0), radii = (0.30fh.x, 0.22fh.y));
eye_r   = ellipse(uv, center + (eye_dx, 0), radii = (0.30fh.x, 0.22fh.y));
eye_zone = max(eye_l, eye_r) * face_ok;

mouth_zone = ellipse(uv, face_center + (0, 0.44fh.y),
                     radii = (0.55fh.x, 0.22fh.y)) * face_ok;
```

`ellipse_zone` dùng falloff mềm `1 - smoothstep(0.72, 1.0, length(d))`.

### 4.1. Teeth

Chỉ pixel trong mouth zone, sáng, ít bão hòa và trung tính:

```hlsl
display = linear_to_srgb(saturate(result));
chroma  = max(r,g,b) - min(r,g,b);
luma    = luminance(display);

neutral = 1 - smoothstep(0.020, 0.075, display.r - display.g);
teeth   = mouth_zone
        * smoothstep(0.42, 0.66, luma)
        * (1 - smoothstep(0.12, 0.28, chroma))
        * neutral
        * teeth_whiten;

white_target = lerp(result, float3(luminance(result)), 0.75) * 1.15;
result = lerp(result, white_target, teeth);
```

Mục tiêu là làm trắng và giảm ám vàng, không làm trắng cả môi hay da quanh miệng. Pixel tối (miệng mở, khoang miệng) không bị tác động.

### 4.2. Eyes

```hlsl
sclera = smoothstep(0.35, 0.65, luma) * (1 - smoothstep(0.10, 0.26, chroma));
dark   = (1 - smoothstep(0.35, 0.60, luma)) * 0.35;
eye_amount = saturate((sclera + dark) * eye_brighten * eye_zone);

eye_target = lerp(result, float3(luminance(result)), 0.20) * 1.40
           + float3(0.015, 0.015, 0.015) * sclera;
result = lerp(result, eye_target, eye_amount);
```

- Lòng trắng (`sclera`) được nâng sáng và giảm ám màu.
- Iris/pupil tối (`dark`) được nâng nhẹ để mắt có hồn hơn.
- Vùng da quanh mắt có thể bị ảnh hưởng nhẹ; giảm `eye_brighten` nếu thấy không tự nhiên.

Cả hai hiệu ứng đều **tắt hoàn toàn** khi `face_valid = 0`.

## 5. Base pass và frequency separation

Hai kernel phối hợp với nhau:

1. **`BlurBase`** render base tần số thấp ở **1/2–1/4 độ phân giải output** vào target RGBA16F. Ở độ phân giải thấp, một blur bán kính lớn chỉ cần 25 mẫu:

   ```hlsl
   fi    = i + 0.5;
   r     = sqrt(fi / 25.0);
   theta = fi * 2.39996323;             // golden angle
   offset = float2(cos(theta), sin(theta)) * r;
   w      = (1.0 - r * 0.55) * exp(-(d*d) / (2*sigma^2));
   ```

   - Bán kính dùng ở base = `radius / divisor`, nên bán kính full-resolution yêu cầu vẫn được giữ đúng.
   - 25 mẫu trải trên Vogel disk, phân bố đều theo góc, tránh alias do các vòng mẫu thưa ở 1440p/4K.
   - `sigma = lerp(0.020, 0.130, 1 - detail)` vẫn áp dụng trọng số bilateral theo chênh lệch màu.

2. **`Draw*`** tái tạo chi tiết ở full resolution: `high_freq = center - base`, tách luma và chroma (mục 6).

Quality chọn divisor: Fast `1/4`, Balanced `1/3`, High `1/2`. Base luôn là **RGBA16F kể cả với SDR** để ảnh linear bị blur không bị lượng tử hoá 8-bit ở vùng tối trước khi encode sRGB. Nếu không tạo được base target, main pass tự fallback về kernel full-resolution [`blur9/13/25`](#fallback-kernel), nên filter vẫn hoạt động trên GPU yếu.

### Vì sao không blur ở full resolution?

Bilateral bán kính lớn ở full resolution cần hàng chục mẫu mỗi pixel, hoặc các vòng mẫu thưa gây alias trên footage độ phân giải cao. Base pass chỉ chạm 1/4–1/16 số pixel, trong khi bước tái tạo vẫn giữ texture full-resolution.

### Vì sao dùng bilateral base thay vì Gaussian?

Gaussian base sẽ chảy qua mắt, môi và viền mặt. Trọng số bilateral giữ các cạnh đó rõ trong khi vẫn làm phẳng các mảng da.

### Fallback kernel

Nếu không có base target, `DrawFast` / `DrawBalanced` / `DrawHigh` dùng kernel full-resolution gốc:

- Fast: 1 vòng, 8 hướng (9 tap gồm center).
- Balanced: vòng trong 0.65r + vòng ngoài 1.35r (trục), 13 tap.
- High: 3 vòng 0.5r / 1.0r / 1.6r, 25 tap.

`step_px = (1/width, 1/height) * radius`; `radius` trong UI được tính ở mốc 1080p và nhân với `height/1080` trong C.

## 6. Tái tạo chi tiết, clarity và complexion

```hlsl
high_freq     = center - base;
luma_detail   = float3(luminance(high_freq));
chroma_detail = high_freq - luma_detail;
detail_gate   = smoothstep(0.0025, 0.0180, abs(luminance(high_freq)));
keep_luma     = lerp(0.12, 0.85, detail);
keep_chroma   = lerp(0.06, 0.50, detail);

smoothed = base + (luma_detail * keep_luma +
                   chroma_detail * keep_chroma) * detail_gate;

mask    = lerp(1.0, skin, skin_detect);
mask   *= lerp(1.0, edge_guard, skin_detect);
result  = lerp(center, smoothed, amount * mask);
```

- **Giữ luma nhiều hơn chroma**: lỗ chân lông, nếp nhăn nhỏ và râu lún phún được giữ, còn mảng đỏ và sắc tố không đều bị làm mịn.
- `detail = 0` giữ 12% chi tiết luma (làm mịn tối đa); `detail = 1` giữ 85%.
- `detail_gate` loại chi tiết dưới mức noise để không khuếch đại noise cảm biến.
- `edge_guard = 1 - smoothstep(0.03, 0.12, abs(luminance(high_freq)))` giảm mask ở vùng nhiều chi tiết.

Clarity dùng **microcontrast 4 láng giềng ở full resolution**, nên không thừa hưởng bán kính blur da lớn và ít halo hơn:

```hlsl
local   = (center + left + right + up + down) * 0.25;
result += (center - local) * clarity * 1.35 *
          (1.0 - mask * 0.85) * local_gate;
```

Complexion, tất cả cân theo `mask`:

```hlsl
result = lerp(result, lerp(float3(luma), result, 1.0 + saturation), mask);
result = lerp(result, result * exp2(brightness * 0.8), mask);

wb = float3(1.0 + warmth * 0.20, 1.0 + warmth * 0.02, 1.0 - warmth * 0.18);
wb /= max(luminance(wb), 0.0001);
result = lerp(result, result * wb, mask);

result += float3(0.080, 0.020, 0.035) * rosy * mask *
          (0.25 + 0.75 * smoothstep(0.05, 0.60, luma));
```

- `brightness` là **hệ số kiểu phơi sáng** thay vì cộng offset, giữ được đặc tính highlight của nguồn.
- `warmth` chuẩn hoá vector white-balance theo luma, nên làm ấm da không kéo theo tăng sáng.
- `rosy` thêm sắc hồng nhẹ, không làm đỏ môi vì đã nhân mask.

Soft glow chỉ lấy vùng sáng, nên vùng tối vẫn sạch:

```hlsl
glow_src = max(base - 0.50, 0.0);
result  += glow_src * (glow * (is_hdr ? 0.65 : 0.45) * mask);
```

## 7. Alpha, color space và HDR

- Texture OBS thường là premultiplied alpha. Helper `sample_linear()` vừa unpremultiply vừa quy đổi về linear tương đối SDR:

  ```hlsl
  float4 src = image.Sample(def_sampler, uv);
  float3 c = src.rgb * hdr_scale / max(src.a, 0.0001);
  ...
  return float4(result * alpha, alpha);
  ```

- Blend state main pass: `ONE / INVSRCALPHA` để giữ đúng alpha.
- `OBS_SOURCE_SRGB` bật để OBS quản lý linear/sRGB nhất quán với các filter core.
- `MaskPass` và `MaskAnalysis` tắt framebuffer sRGB; main pass bật lại.
- **Anti-banding**: với định dạng 8-bit, shader thêm nhiễu tam giác 1 LSB trong display space sau khi xử lý:
  ```hlsl
  d  = linear_to_srgb(saturate(result));
  d += (n1 + n2 - 1.0) * (1.0 / 255.0);
  result = srgb_to_linear(saturate(d));
  ```
  Bước này bỏ qua cho mask/zone preview và nửa gốc của split view. Target HDR/16F không bị dither.
- **HDR**:
  - `GS_CS_709_EXTENDED` và `GS_CS_SRGB_16F`: `hdr_scale = 1.0`.
  - `GS_CS_709_SCRGB`: `hdr_scale = 80 / obs_get_video_sdr_white_level()` (fallback 300 nếu giá trị không hợp lệ).
  - `sample_linear()` nhân `hdr_scale` đúng **một lần**; mọi phép tính mask/tone dùng linear tương đối SDR. **Không `saturate`** khi `is_hdr`, rồi chia lại `hdr_scale` một lần trước khi ghi.
  - Nhờ vậy highlight > 1.0 vẫn được giữ và vẫn đi qua filter, thay vì bị bypass như phiên bản 1.0.
- Capture dùng **output space** (`gs_get_color_space()`), không dùng `source_space`: OBS sẽ tự chuyển đổi/tonemap nguồn vào đúng space của target, tránh double-scale đặc biệt với scRGB. `src_format` quyết định `gs_effect_set_texture_srgb()` hay `gs_effect_set_texture()`.
- Trước main pass, `p_image` được **rebind lại** vì một số backend (OpenGL) reset texture unit khi đổi technique. Đây là bug đã gặp và đã sửa; không được bỏ bước này.

## 8. Preview

- **Mask preview**: trả `float3(mask)` (premultiplied), cho thấy vùng da trắng và vùng khác đen.
- **Split preview**: `uv.x < 0.5` trả màu gốc; nửa phải là kết quả.
- **Zone preview**: tô face prior xanh lá, eye zones xanh dương và mouth zone vàng, dùng để kiểm tra face box.
- Preview được đặt sau opacity để vẫn nhìn thấy mask dù opacity = 0 (trừ split).

## 9. An toàn luồng và tài nguyên

- `create`, `destroy`, `update` chạy trên main/UI thread; các thao tác graphics được bọc `obs_enter_graphics()` / `obs_leave_graphics()`.
- `render` chạy trên graphics thread, chỉ đọc struct đã cập nhật; không cấp phát bộ nhớ mỗi frame.
- Effect được giữ suốt vòng đời filter.
- `src_texrender`, mask/analysis texrender và stage surface được tạo lại khi width/height/color space đổi; ping-pong mask index reset cùng lúc.
- Nếu effect không load được, `create` trả `NULL` và ghi warning; OBS sẽ không thêm filter hỏng.
- Nếu allocation thất bại, filter `skip_video_filter()` thay vì crash.

## 10. Các thay thế đã cân nhắc

| Phương án | Ưu | Nhược |
|---|---|---|
| Gaussian blur + blend | Rẻ, dễ | Làm mờ cạnh, da bệt |
| Bilateral một pass (1.0) | Rẻ, giữ cạnh | Không có temporal/face zone |
| Multi-pass hiện tại | Mask ổn định, face zone, HDR | Thêm mask pass + readback nhỏ |
| Separable bilateral 2 pass | Chất lượng/chi phí tốt hơn | Cần thêm texrender, quản lý color space |
| Guided filter | Rất mượt, giữ cạnh tốt | Cần box filter nhiều pass, phức tạp |
| ML segmentation | Mask chính xác, multi-face | Model nặng, thêm dependency, khó ship |

Hướng phát triển: preset, compute shader cho radius lớn, đo GPU time, và tuỳ chọn ML landmark.

## 11. Khóa settings

| Khóa | Kiểu | Ghi chú |
|---|---|---|
| `preset` | string | `custom` hoặc id preset có sẵn; chỉ là lựa chọn trên UI |
| `amount` | double | 0..1 |
| `radius` | double | px @1080p |
| `detail` | double | 0..1 |
| `skin_detect` | bool | |
| `skin_sensitivity` | double | 0..1 |
| `mask_preview` | bool | |
| `brightness` | double | -50..50 |
| `warmth` | double | -50..50 |
| `saturation` | double | -50..50 |
| `rosy` | double | 0..1 |
| `clarity` | double | 0..1 |
| `glow` | double | 0..1 |
| `face_aware` | bool | bật face prior + zones |
| `temporal_strength` | double | 0..1 |
| `teeth_whiten` | double | 0..1 |
| `eye_brighten` | double | 0..1 |
| `zone_preview` | bool | |
| `face_manual` | bool | |
| `face_cx` | double | 0..1 |
| `face_cy` | double | 0..1 |
| `face_w` | double | 0.02..1, full width |
| `face_h` | double | 0.02..1, full height |
| `quality` | string | `fast`/`balanced`/`high` |
| `split_view` | bool | |
| `opacity` | double | 0..1 |
| `anti_banding` | bool | dithering cho ngõ ra 8-bit |

`src/beauty-presets.c` quản lý bảng preset có sẵn. Khi áp preset, plugin ghi các khóa tinh chỉnh ở trên nhưng không ghi `preset`; callback của property lưu lựa chọn và chuyển sang `custom` sau lần chỉnh tay đầu tiên.
