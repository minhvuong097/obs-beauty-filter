# Hướng dẫn tinh chỉnh

**[English](TUNING.en.md) | Tiếng Việt**

## 1. Thứ tự thiết lập nên theo

1. **Ánh sáng và white balance trước.** Beauty filter không sửa được ánh sáng tím/xanh.
2. **Denoise trước Beauty** nếu webcam noise nhiều. Smoothing trên nền noise sẽ tạo vệt.
3. **Skin mask**: bật **Show skin mask**, chỉnh **Skin sensitivity** cho đến khi vùng da (mặt, cổ, tay) trắng gần như hoàn toàn, còn mắt/môi/tóc/nền đen.
4. **Face-aware zones**: bật vài giây, bật **Show face zones** để chắc face box, hai mắt và miệng nằm đúng. Nếu sai, dùng **Manual face box**.
5. **Amount + Radius**: tăng đến khi vừa ý, sau đó giảm 10–20%.
6. **Detail preservation**: tăng nếu da bệt; giảm nếu vẫn còn mụn/texture.
7. **Clarity**: tăng nhẹ để mắt/tóc nét.
8. **Complexion**: chỉnh sáng/ấm/hồng ở mức nhỏ.
9. **Temporal mask stability**: tăng nếu mask nhấp nháy; giảm nếu thấy bóng mờ khi đầu quay nhanh.
10. **Teeth whitening / Eye brightening**: bật sau cùng, ở mức nhỏ, và kiểm tra bằng **Show face zones**.
11. **Split preview** để kiểm tra, rồi tắt trước khi lên sóng.

## 2. Preset gợi ý

Bảng điều khiển bắt đầu bằng menu **Preset khởi đầu**. Chọn mục gần với camera và ánh sáng của bạn, sau đó tinh chỉnh các nút bên dưới. Chỉnh bất kỳ thông số sáng tạo nào sẽ tự chuyển menu về **Tùy chỉnh (giữ thông số hiện tại)**; các con số trong mục này là giá trị chính xác mà mỗi preset áp dụng.

**Mặc định** khôi phục thiết lập gốc của plugin (Amount 0.35, Radius 6, Detail 0.35, sensitivity 0.50, Rosy 0.15, Clarity 0.10, Glow 0.05, temporal 0.60, chất lượng Balanced, bật chống dải màu). Dùng preset này làm mốc sạch trước khi chọn preset theo camera.

### Natural — webcam 1080p, đèn trong nhà

| Thông số | Giá trị |
|---|---:|
| Amount | 0.35 |
| Radius | 6 |
| Detail | 0.40 |
| Sensitivity | 0.50 |
| Brightness | 0 |
| Warmth | 3 |
| Saturation | 0 |
| Rosy | 0.12 |
| Clarity | 0.10 |
| Glow | 0.05 |
| Face-aware zones | On |
| Temporal stability | 0.60 |
| Teeth / Eyes | 0 / 0 |
| Quality | Balanced |

### Cinema — camera cao cấp 1440p/4K

| Thông số | Giá trị |
|---|---:|
| Amount | 0.28 |
| Radius | 8 |
| Detail | 0.55 |
| Sensitivity | 0.50 |
| Brightness | 0 |
| Warmth | 2 |
| Saturation | 0 |
| Rosy | 0.10 |
| Clarity | 0.08 |
| Glow | 0.04 |
| Face-aware zones | On |
| Temporal stability | 0.65 |
| Teeth / Eyes | 0.20 / 0.12 |
| Quality | High |
| Anti-banding | On |

Cảm biến cao cấp đã có nhiều texture, nên dùng Detail cao hơn và Amount thấp hơn preset webcam 1080p. Base 1/2 độ phân giải giữ lỗ chân lông, nếp nhăn nhỏ trong khi làm đều các mảng da lớn. Nếu GPU không đủ headroom, hạ Quality xuống `Balanced` thay vì giảm Detail.

### Studio — close-up, GPU rời

| Thông số | Giá trị |
|---|---:|
| Amount | 0.60 |
| Radius | 10 |
| Detail | 0.28 |
| Sensitivity | 0.50 |
| Brightness | 4 |
| Warmth | 2 |
| Saturation | 2 |
| Rosy | 0.18 |
| Clarity | 0.12 |
| Glow | 0.10 |
| Face-aware zones | On |
| Temporal stability | 0.55 |
| Teeth / Eyes | 0.35 / 0.20 |
| Quality | High |

### Soft — da khô, ánh sáng mềm

| Thông số | Giá trị |
|---|---:|
| Amount | 0.45 |
| Radius | 8 |
| Detail | 0.30 |
| Sensitivity | 0.55 |
| Brightness | 3 |
| Warmth | 4 |
| Saturation | 4 |
| Rosy | 0.20 |
| Clarity | 0.08 |
| Glow | 0.18 |
| Face-aware zones | On |
| Temporal stability | 0.65 |
| Teeth / Eyes | 0.25 / 0.15 |
| Quality | Balanced |

### Nam giới / có râu

| Thông số | Giá trị |
|---|---:|
| Amount | 0.22 |
| Radius | 5 |
| Detail | 0.60 |
| Sensitivity | 0.45 |
| Brightness | 0 |
| Warmth | 0 |
| Saturation | 0 |
| Rosy | 0.05 |
| Clarity | 0.12 |
| Glow | 0 |
| Face-aware zones | On |
| Temporal stability | 0.50 |
| Teeth / Eyes | 0.20 / 0.10 |
| Quality | Balanced |

### Da sẫm màu

| Thông số | Giá trị |
|---|---:|
| Amount | 0.30 |
| Radius | 6 |
| Detail | 0.45 |
| Sensitivity | 0.65–0.75 |
| Brightness | 3–6 |
| Warmth | 0–3 |
| Saturation | 2–5 |
| Rosy | 0.05–0.10 |
| Clarity | 0.10 |
| Glow | 0.05 |
| Face-aware zones | On |
| Temporal stability | 0.60 |
| Teeth / Eyes | 0.30 / 0.15 |
| Quality | Balanced |

Luôn kiểm tra **Show skin mask**: nếu vùng da tối không trắng, tăng Sensitivity; nếu tóc/nền cũng trắng, giảm.

### HDR — Rec.709 extended / scRGB

| Thông số | Giá trị |
|---|---:|
| Amount | 0.25–0.40 |
| Radius | 4–7 |
| Detail | 0.40–0.55 |
| Glow | 0–0.05 |
| Face-aware zones | On |
| Temporal stability | 0.50–0.65 |
| Quality | High (hoặc Balanced nếu GPU yếu) |

HDR có dải sáng rộng hơn, nên Glow và Rosy dễ trông quá tay. Bắt đầu thấp hơn preset SDR, kiểm tra highlight (đèn, cửa sổ) để chắc không bị cháy. `GS_CS_SRGB_16F` dùng cùng đường xử lý với Rec.709 extended; scRGB được xử lý theo thang 80 nit.

## 3. Tinh chỉnh face zones

### Khi nào cần bật?

- Bật **Face-aware zones** nếu dùng **Teeth whitening** hoặc **Eye brightening**.
- Face prior cũng giúp giảm tường/gỗ màu da bị nhận nhầm.
- Tắt nếu không dùng teeth/eye và muốn giảm chi phí GPU.

### Kiểm tra face box

1. Bật **Show face zones**.
2. Màu mong đợi:
   - **xanh lá**: face prior (vùng mặt + cổ),
   - **xanh dương**: hai ellipse mắt,
   - **vàng**: ellipse miệng.
3. Nếu box lệch:
   - Bật **Manual face box**.
   - Chỉnh **Face center X/Y** để đưa tâm vào giữa mặt.
   - Chỉnh **Face width/height** cho vừa mặt. Đây là **kích thước đầy đủ**, không phải bán kính.
4. Tắt **Show face zones** sau khi chỉnh.

### Liều lượng khuyến nghị

- **Teeth whitening**: `0.2–0.5`. Trên `0.6` dễ làm môi/nướu bị nhạt màu.
- **Eye brightening**: `0.1–0.3`. Trên `0.4` dễ tạo mắt "trắng bệch" hoặc quầng sáng quanh mắt.
- **Temporal stability**: `0.4–0.7` là vùng an toàn. Trên `0.8` có thể thấy bóng mờ khi đầu quay nhanh; dưới `0.3` mask dễ nhấp nháy.
- Nếu da quanh mắt/miệng cũng sáng lên: giảm Teeth/Eyes trước, sau đó thu nhỏ face box.

## 4. Xử lý sự cố

| Hiện tượng | Nguyên nhân thường gặp | Cách sửa |
|---|---|---|
| Da bị bệt, mất texture | Amount/Radius quá cao, Detail quá thấp | Giảm Amount, giảm Radius, tăng Detail |
| Mắt/tóc mờ | Clarity thấp, Radius cao | Tăng Clarity, giảm Radius |
| Nền gỗ/tường cam bị mịn | Mask bắt nhầm màu da | Bật Show skin mask; giảm Sensitivity; bật Face-aware zones; dùng Manual face box nếu cần |
| Môi bị nhạt/mờ | Mask phủ môi | Giảm Sensitivity; giảm Amount; thêm Clarity |
| Da ngả đỏ/hồng quá | Rosy/Warmth cao | Giảm Rosy, Warmth |
| Da ngả vàng/xanh | White balance nguồn sai | Sửa white balance trước; dùng Warmth nhẹ |
| Nhấp nháy vùng da khi chuyển động | Temporal thấp hoặc mask per-pixel thay đổi | Tăng Temporal stability (0.6–0.75); giảm Amount; tăng Detail |
| Bóng mờ/ghosting khi quay đầu | Temporal quá cao | Giảm Temporal stability (0.3–0.5) |
| Teeth không có tác dụng | Chưa có face box, răng tối/ám màu, hoặc zone sai | Bật Face-aware zones; bật Show face zones; dùng Manual face box; tăng Teeth vừa phải |
| Teeth làm trắng cả môi | Zone miệng quá lớn hoặc Teeth quá cao | Thu nhỏ Face height/width; giảm Teeth |
| Eye brightening làm sáng cả da | Zone mắt quá lớn hoặc Eyes quá cao | Giảm Eyes; thu nhỏ Face width/height; kiểm tra zone preview |
| Face box nhảy lung tung | Nhiều người, nền màu da, mặt bị che | Dùng Manual face box; giảm Sensitivity; tắt Face-aware nếu không dùng teeth/eye |
| Tụt FPS | Quality/Radius cao, 4K, face-aware bật | Hạ Quality, giảm Radius, tắt Glow, tắt Face-aware nếu không dùng |
| Highlight HDR bị cháy | Glow/Rosy quá cao hoặc nguồn đã cháy | Giảm Glow/Rosy; kiểm tra nguồn; filter không tone-map |
| Không thấy tác dụng | Amount/Opacity = 0, hoặc source không hỗ trợ color space | Kiểm tra Amount/Opacity; xem log OBS |
| Filter không xuất hiện | Sai vị trí DLL/data | Kiểm tra layout cài đặt, xem log OBS |

## 5. Lưu ý khi làm việc với nhiều tông da

- Không dùng một preset duy nhất cho mọi người.
- Kiểm tra mask ở 3 điều kiện: sáng mạnh, thiếu sáng, và ngược sáng.
- Tránh đặt Amount > 0.6 cho da sẫm màu nếu chưa kiểm tra kỹ; dễ làm mất chi tiết.
- Với da rất sáng, giảm Brightness và Rosy để tránh cháy highlight.
- Với da sẫm, tăng Sensitivity nhưng kiểm tra face box; face prior có thể cắt mất vùng da ngoài ellipse nếu box quá nhỏ.
- Nếu stream có nhiều người, dùng filter riêng cho từng camera/source thay vì một filter chung cho cả scene. Detector hiện chỉ xử lý một face box.

## 6. Thứ tự filter khuyến nghị

```
Camera → Denoise → Color Correction (nếu cần) → Beauty Filter → Sharpen (rất nhẹ) → Scene
```

- Đặt Beauty **sau** denoise để không làm mờ noise thành mảng.
- Đặt Sharpen **sau** Beauty nếu muốn bù chi tiết, nhưng chỉ 0.05–0.15.
- Không stack nhiều filter làm mịn (Beauty + Blur + Sharpen) vì dễ tạo halo.
