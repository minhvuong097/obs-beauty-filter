# OBS Beauty Filter Architecture

**English | [Tiếng Việt](ARCHITECTURE.md)**

This document describes the processing flow, shader maths and design decisions of plugin **1.5.0** that keep it stable inside OBS' filter pipeline.

## 1. Filter lifecycle and render pipeline

The plugin registers one `obs_source_info`:

```c
.id = "beauty_filter"
.type = OBS_SOURCE_TYPE_FILTER
.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB
```

Each frame, `beauty_render()`:

1. Resolves the target/parent with `obs_filter_get_target()` / `obs_filter_get_parent()`.
2. Queries the preferred colour spaces:

   ```c
   GS_CS_SRGB, GS_CS_SRGB_16F, GS_CS_709_EXTENDED, GS_CS_709_SCRGB
   ```

   Any other colour space (for example an unmapped HLG/PQ source) is passed through with `obs_source_skip_video_filter()`.
3. Lazily allocates `src_texrender`, two `mask_texrender`s, `analysis_texrender`, `base_texrender`, `analysis_stage` and `white_tex` when size, colour space or quality changes.
4. **Capture**: renders the target into `src_texrender` with
   `gs_texrender_begin_with_color_space(..., gs_get_color_space())`. Using the **output space** lets OBS perform source conversion/tonemapping (including scRGB) and avoids double scaling; it uses `obs_source_default_render()` for ordinary inputs and `obs_source_video_render()` for custom/async sources.
5. **MaskPass** (when a mask is needed): renders a half-resolution mask into `mask_texrender[mask_index]`, sampling history from the other texture.
6. **MaskAnalysis** (periodic): downsamples the mask to a 64×N grid and performs a small readback to update the face box.
7. **BlurBase**: renders the low-resolution bilateral base into `base_texrender` (1/2–1/4 resolution, RGBA16F).
8. **Main pass**: selects `DrawFast` / `DrawBalanced` / `DrawHigh`, sets all uniforms and draws a fullscreen quad, reconstructing full-resolution detail on top of the base.
9. Restores blend state, framebuffer sRGB and linear sRGB.

`obs_source_process_filter_begin_with_color_space()` is not used because the plugin needs control over capture, the mask pass and colour-space handling. Manual capture remains compatible with the filter chain because target/parent come from OBS.

The effect is created once in `create()` with `gs_effect_create_from_file()` and destroyed in `destroy()`. No effect is created in `render()`.

## 2. Skin mask pass

`MaskPass` runs at **half resolution** (`(width+1)/2 × (height+1)/2`) in RGBA8:

- `R` = raw mask (used by the face detector, independent of the face prior).
- `G` = stabilised mask (used by the main pass).
- `B` = 0, `A` = 1.

### 2.1. YCbCr

```hlsl
Y  = 0.299R + 0.587G + 0.114B
Cb = -0.168736R - 0.331264G + 0.5B + 0.5
Cr =  0.5R - 0.418688G - 0.081312B + 0.5
```

Human skin lies around a chroma ellipse. The ellipse **shifts with luma** because darker skin has higher Cb and lower Cr than light skin:

- dark skin: centre `(Cb 0.455, Cr 0.555)`, radii `(0.060, 0.065)`
- light skin: centre `(Cb 0.385, Cr 0.600)`, radii `(0.100, 0.090)`
- interpolated with `smoothstep(0.05, 0.55, Y)`

```hlsl
float t = smoothstep(0.05, 0.55, Y);
float2 center = lerp(float2(0.455, 0.555), float2(0.385, 0.600), t);
float2 radii  = lerp(float2(0.060, 0.065), float2(0.100, 0.090), t);
float2 d = (float2(cb, cr) - center) / radii;
float chroma = 1.0 - smoothstep(0.55, 1.30, length(d));
```

The mask is evaluated on **sRGB** values (`linear_to_srgb(...)`) because the shader runs with `OBS_SOURCE_SRGB` and receives linear-light samples. Tone/glow operations remain in linear light.

### 2.2. Luma gate and red dominance

```hlsl
luma_gate = smoothstep(0.015, 0.080, Y) * (1 - smoothstep(0.95, 1.0, Y));
red_dom   = smoothstep(-0.020, 0.060, R-B) * smoothstep(-0.035, 0.035, R-G);
```

- `luma_gate` rejects very dark pixels (hair, eyes, black backgrounds) and blown highlights.
- `red_dom` rejects blue/green backgrounds and reduces false positives on deeply red lips.

### 2.3. Sensitivity

```hlsl
p = saturate(chroma * luma_gate * red_dom);
mask = pow(p, lerp(1.70, 0.55, sensitivity));
```

- `sensitivity = 0`: tight mask, few false positives.
- `sensitivity = 1`: wider mask, better coverage but also picks up skin-coloured wood/walls.

### 2.4. Neighbourhood coherence

The initial mask is checked against 8 neighbours (about `1.5 texel` away; diagonal samples scaled by `0.7071`):

```hlsl
support = average(skin_probability(uv + offset));
coherence = smoothstep(0.10, 0.45, support);
raw = center_p * lerp(0.55, 1.0, coherence) * texture_guard;
```

- Isolated skin-coloured pixels (noise, stray dots) are strongly attenuated.
- Continuous skin keeps almost full weight.
- This is the key step that removes small skin-coloured specks on the background.

### 2.5. Gradient/texture guard

Luma of four neighbouring pixels gives a gradient:

```hlsl
gx = Y(right) - Y(left);
gy = Y(up)    - Y(down);
gradient = sqrt(gx*gx + gy*gy);
texture_guard = 1.0 - 0.85 * smoothstep(0.035, 0.16, gradient);
```

Wood grain, hair strands, beards and object edges have a high gradient, so the mask is reduced; flat skin is almost unaffected.

### 2.6. Temporal stabilisation

`MaskPass` reads the previous stabilised mask from `history_tex.g` (ping-pong between the two `mask_texrender`s):

```hlsl
previous = history_tex.g;
motion   = smoothstep(0.10, 0.45, abs(raw - previous));
keep     = temporal_strength * (1.0 - motion) * history_valid;
stabilized = lerp(raw, previous, keep);
```

- Static regions keep history → less flicker.
- Strong changes (head turn, scene cut) trust the current frame → avoids ghosting.
- `history_valid = 0` on the first frame or after a size/colour-space change.
- `temporal_strength = 0` disables temporal blending entirely.

### 2.7. Face prior

With a valid face box, the stabilised mask is multiplied by a soft ellipse covering the face and neck:

```hlsl
center = face_center + float2(0.0, face_size.y * 0.22);
radii  = max(face_size * float2(1.15, 1.55), 0.0001);
prior  = 1.0 - smoothstep(0.85, 1.20, length((uv - center) / radii));
stabilized *= lerp(1.0, prior, 0.90 * face_valid * face_aware);
```

The face prior is applied **only to the stabilised mask**, never to the raw mask. This keeps the detector (which reads the raw mask) independent of its own output, so it cannot lock itself into a wrong box.

## 3. Heuristic face region

### 3.1. Why not ML?

The goal is a lightweight plugin with no model and no dependency. A heuristic detector is good enough for a single-person webcam, and a **manual face box** is always available as a fallback. ML landmarks/segmentation are left on the roadmap.

### 3.2. Grid and readback

- `MaskAnalysis` downsamples the raw mask to a **64 × N** grid (N proportional to aspect, clamped 16..64) with a 4×4 box filter (16 taps).
- The grid is staged and mapped to the CPU. Maximum size is 64×64 RGBA = 16 KB.
- It only runs when `face_aware` is on, `face_manual` is off, and `frame_counter % 6 == 0` → about 5 times per second at 30 fps.
- The full-resolution frame is never read back.

### 3.3. Connected components and face box

`face-region.c`:

1. Scan the grid and flood-fill 4-connected pixels with `R >= 64` into components.
2. Discard components with an area below 8.
3. For each component, take the upper band from **6% to 34%** of its height (forehead and upper cheeks).
4. The band must have:
   - width between 4% and 80% of frame width,
   - skin density of at least 22%,
   - at least 6 pixels,
   - horizontal centre within 8%..92% of the frame.
5. Estimate:

   ```text
   face_w = band_w * 1.10
   face_h = face_w * 1.35          (clamped to ≤ 98% frame height, ≥ 10%)
   face_cx = horizontal centre of the band
   face_cy = component top + face_h / 2
   half_w = face_w * 0.5 * 1.12
   half_h = face_h * 0.5 * 1.12
   ```

6. Pick the highest-scoring component:

   ```text
   score = area
         * smoothstep(0.22, 0.55, density)
         * (0.55 + 0.45 * smoothstep(0.12, 0.45, band_w / component_h))
         * (1 - 0.70 * |face_cx - 0.5|)
         * (1 - 0.35 * face_cy)
   ```

   The score prefers large, dense components with a plausible aspect ratio, near the upper centre of the frame.

### 3.4. Temporal smoothing and hold

- On a successful detection, the box is smoothed towards the new one:

  ```c
  box += (new_box - box) * 0.35f;
  ```

- On a failed detection, the old box is held for at most **5 updates** before being invalidated. This avoids losing teeth/eye zones when the face turns slightly or is briefly occluded.
- With `face_manual` on, automatic detection is skipped and the manual box (`face_cx`, `face_cy`, `face_w`, `face_h`) is used directly. `face_w/h` are full, normalised dimensions.

### 3.5. Limitations

The detector cannot distinguish a face from an arm or skin-coloured clothing, does not handle multiple faces, and may pick a larger component (for example a wall). Therefore:

- The face prior is a soft multiplier (`90%`), not a hard mask.
- Teeth/eye processing only runs with a valid box.
- The manual box is the supported fallback.

## 4. Teeth whitening and eye brightening

Both zones are derived from the face box (half-size `fh = face_size`):

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

`ellipse_zone` uses a soft falloff `1 - smoothstep(0.72, 1.0, length(d))`.

### 4.1. Teeth

Only bright, low-chroma, neutral pixels inside the mouth zone:

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

The goal is to whiten teeth and reduce yellow cast without brightening lips or surrounding skin. Dark pixels (open mouth, oral cavity) are untouched.

### 4.2. Eyes

```hlsl
sclera = smoothstep(0.35, 0.65, luma) * (1 - smoothstep(0.10, 0.26, chroma));
dark   = (1 - smoothstep(0.35, 0.60, luma)) * 0.35;
eye_amount = saturate((sclera + dark) * eye_brighten * eye_zone);

eye_target = lerp(result, float3(luminance(result)), 0.20) * 1.40
           + float3(0.015, 0.015, 0.015) * sclera;
result = lerp(result, eye_target, eye_amount);
```

- The sclera is brightened and desaturated.
- The dark iris/pupil receives a modest lift for a livelier look.
- Skin around the eyes may be slightly affected; lower `eye_brighten` if it looks unnatural.

Both effects are **completely disabled** when `face_valid = 0`.

## 5. Base pass and frequency separation

Two kernels work together:

1. **`BlurBase`** renders the low-frequency base at **1/2–1/4 of the output resolution** into an RGBA16F target. At reduced resolution a wide blur needs only 25 samples:

   ```hlsl
   fi    = i + 0.5;
   r     = sqrt(fi / 25.0);
   theta = fi * 2.39996323;             // golden angle
   offset = float2(cos(theta), sin(theta)) * r;
   w      = (1.0 - r * 0.55) * exp(-(d*d) / (2*sigma^2));
   ```

   - The radius used at base resolution is `radius / divisor`, so the requested full-resolution radius is preserved.
   - The 25 taps are spread over a Vogel disk, which is rotationally even and avoids the sparse-ring aliasing of a full-resolution kernel at 1440p/4K.
   - `sigma = lerp(0.020, 0.130, 1 - detail)` still applies the bilateral edge weight.

2. **`Draw*`** reconstructs detail at full resolution: `high_freq = center - base`, split into luma and chroma (section 6).

Quality selects the divisor: Fast `1/4`, Balanced `1/3`, High `1/2`. The base is **RGBA16F even for SDR** so a blurred linear image is not quantised to 8 bits in the shadows before the sRGB encode. If the base target cannot be created, the main pass falls back to the full-resolution [`blur9/13/25`](#fallback-kernels) kernel, so the filter still works on constrained GPUs.

### Why not blur at full resolution?

A wide-radius bilateral at full resolution needs dozens of taps per pixel or sparse rings that alias on high-resolution footage. The base pass touches only 1/4–1/16 of the pixels, while detail reconstruction keeps the full-resolution texture.

### Why a bilateral base and not a Gaussian?

A Gaussian base bleeds across eyes, lips and the face outline. The bilateral range weight keeps those edges defined while still flattening skin blotches.

### Fallback kernels

If the base target is unavailable, `DrawFast` / `DrawBalanced` / `DrawHigh` use the original full-resolution kernels:

- Fast: one ring, 8 directions (9 taps including centre).
- Balanced: inner 0.65r + outer 1.35r (axis), 13 taps.
- High: three rings 0.5r / 1.0r / 1.6r, 25 taps.

`step_px = (1/width, 1/height) * radius`, and the UI `radius` is defined at 1080p and scaled by `height/1080` in C.

## 6. Detail reconstruction, clarity and complexion

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

- **Luma is retained more than chroma**: pores, fine lines and beard stubble survive while red blotches and uneven skin tone are smoothed.
- `detail = 0` keeps 12% of luma detail (maximum smoothing); `detail = 1` keeps 85%.
- `detail_gate` removes sub-noise-level detail so sensor noise is not amplified.
- `edge_guard = 1 - smoothstep(0.03, 0.12, abs(luminance(high_freq)))` lowers the mask on highly detailed areas.

Clarity uses a **full-resolution 4-neighbour microcontrast**, so it does not inherit the large skin-blur radius and produces fewer halos:

```hlsl
local   = (center + left + right + up + down) * 0.25;
result += (center - local) * clarity * 1.35 *
          (1.0 - mask * 0.85) * local_gate;
```

Complexion, all weighted by `mask`:

```hlsl
result = lerp(result, lerp(float3(luma), result, 1.0 + saturation), mask);
result = lerp(result, result * exp2(brightness * 0.8), mask);

wb = float3(1.0 + warmth * 0.20, 1.0 + warmth * 0.02, 1.0 - warmth * 0.18);
wb /= max(luminance(wb), 0.0001);
result = lerp(result, result * wb, mask);

result += float3(0.080, 0.020, 0.035) * rosy * mask *
          (0.25 + 0.75 * smoothstep(0.05, 0.60, luma));
```

- `brightness` is an **exposure-like multiplier** rather than an additive offset, preserving the source's highlight rolloff.
- `warmth` normalises the white-balance vector by its luma, so warming the skin does not simply brighten it.
- `rosy` adds a light pink tint without reddening the lips because it is mask-weighted.

Soft glow only picks up highlights, so shadows stay clean:

```hlsl
glow_src = max(base - 0.50, 0.0);
result  += glow_src * (glow * (is_hdr ? 0.65 : 0.45) * mask);
```

## 7. Alpha, colour space and HDR

- OBS textures are usually premultiplied alpha. The `sample_linear()` helper both unpremultiplies and converts to SDR-relative linear:

  ```hlsl
  float4 src = image.Sample(def_sampler, uv);
  float3 c = src.rgb * hdr_scale / max(src.a, 0.0001);
  ...
  return float4(result * alpha, alpha);
  ```

- Main-pass blend state: `ONE / INVSRCALPHA` to preserve alpha.
- `OBS_SOURCE_SRGB` keeps OBS' linear/sRGB management consistent with core filters.
- `MaskPass` and `MaskAnalysis` disable framebuffer sRGB; the main pass re-enables it.
- **Anti-banding**: for 8-bit formats the shader adds triangular noise of one LSB in display space after processing:
  ```hlsl
  d  = linear_to_srgb(saturate(result));
  d += (n1 + n2 - 1.0) * (1.0 / 255.0);
  result = srgb_to_linear(saturate(d));
  ```
  It is skipped for mask/zone previews and for the original half of split view. HDR/16F targets are not dithered.
- **HDR**:
  - `GS_CS_709_EXTENDED` and `GS_CS_SRGB_16F`: `hdr_scale = 1.0`.
  - `GS_CS_709_SCRGB`: `hdr_scale = 80 / obs_get_video_sdr_white_level()` (fallback 300 if the value is invalid).
  - `sample_linear()` applies `hdr_scale` exactly **once**; all mask/tone maths work in SDR-relative linear. It does **not `saturate`** when `is_hdr`, and divides by `hdr_scale` once before writing.
  - Highlights above 1.0 are preserved and still filtered instead of being bypassed as in 1.0.
- Capture uses the **output space** (`gs_get_color_space()`), not `source_space`: OBS converts/tonemaps the source into the target's space, which avoids double scaling (especially for scRGB). `src_format` decides between `gs_effect_set_texture_srgb()` and `gs_effect_set_texture()`.
- Before the main pass, `p_image` is **rebound** because some backends (OpenGL) reset texture unit bindings when switching techniques. This was a real bug and must not be removed.

## 8. Previews

- **Mask preview**: returns `float3(mask)` (premultiplied), showing skin as white and other areas as black.
- **Split preview**: `uv.x < 0.5` returns the original; the right half is the result.
- **Zone preview**: tints the face prior green, eye zones blue and mouth zone yellow, for checking the face box.
- Previews are applied after opacity so the mask remains visible at opacity 0 (except split).

## 9. Thread and resource safety

- `create`, `destroy` and `update` run on the main/UI thread; graphics work is wrapped in `obs_enter_graphics()` / `obs_leave_graphics()`.
- `render` runs on the graphics thread and only reads the updated struct; it does not allocate per frame.
- The effect lives for the whole filter lifetime.
- `src_texrender`, mask/analysis texrenders and the stage surface are recreated when width/height/colour space changes; the ping-pong mask index resets at the same time.
- If the effect fails to load, `create` returns `NULL` and logs a warning; OBS will not add a broken filter.
- If allocation fails, the filter calls `skip_video_filter()` instead of crashing.

## 10. Alternatives considered

| Approach | Pros | Cons |
|---|---|---|
| Gaussian blur + blend | Cheap, simple | Blurs edges, flat skin |
| Single-pass bilateral (1.0) | Cheap, edge-preserving | No temporal or face zones |
| Current multi-pass | Stable mask, face zones, HDR | Extra mask pass + tiny readback |
| Separable 2-pass bilateral | Better quality/cost | More texrenders, colour-space handling |
| Guided filter | Very smooth, strong edge preservation | Multi-pass box filter, complex |
| ML segmentation | Accurate mask, multi-face | Heavy model, dependency, hard to ship |

Future direction: presets, compute shaders for large radii, GPU timing, and optional ML landmarks.

## 11. Settings keys

| Key | Type | Notes |
|---|---|---|
| `preset` | string | `custom`, a built-in preset id, or `custom:<name>`; UI selection only |
| `preset_name` | string | last custom preset name typed in the UI; not saved into the preset file |
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
| `face_aware` | bool | enables face prior + zones |
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
| `anti_banding` | bool | 8-bit output dithering |

`src/beauty-presets.c` owns the built-in preset table. Applying a preset writes the tuning keys above but never writes `preset` itself; the property callback stores the selection and switches it to `custom` after the first manual edit.

## 12. Custom preset storage

`src/beauty-custom-presets.c` keeps user presets in a single JSON file in the
OBS module config directory (`custom-presets.json`). The file is intentionally
small and human-editable:

```json
{
  "version": 1,
  "presets": [
    {
      "name": "My Look",
      "amount": 0.35,
      "radius": 6.0,
      "detail": 0.35,
      "skin_detect": true,
      "...": "the same creative keys as the built-in presets"
    }
  ]
}
```

- The in-memory list is loaded lazily and cached; `reload()` drops the cache.
- Writes use `obs_data_save_json_safe()` (`.tmp` + `.bak`), so a crash during a
  save does not corrupt the previous file.
- Names are trimmed, limited to 64 characters, and compared
  case-insensitively. They cannot match `custom` or a built-in preset id.
- Applying a preset fills missing keys from the built-in Default preset, which
  keeps older or hand-edited files usable.
- Tests override the path with `OBS_BEAUTY_CUSTOM_PRESET_PATH`; production
  builds resolve it through `obs_module_config_path()`.
- The UI stores a transient status message in `struct beauty_filter`
  (`preset_status`), never in `obs_data`, so status text is not saved with the
  scene collection.

The property callback receives the filter through
`obs_properties_create_param(filter, NULL)` and
`obs_properties_get_param()`. This is why the property page can update the
source settings and refresh itself from a button callback.
