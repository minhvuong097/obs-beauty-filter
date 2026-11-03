# Testing

**English | [Tiếng Việt](TESTING.md)**

## 1. Build checks

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

If you use Visual Studio 2022, replace the generator with
`"Visual Studio 17 2022"`. On Windows, `BEAUTY_OBS33_PLUGIN_LAYOUT` defaults to
`ON` and staging uses the OBS 33+ layout. Add
`-DBEAUTY_OBS33_PLUGIN_LAYOUT=OFF` to test the legacy layout for OBS 32 and
older.

## 2. Face-region unit test

Pure CPU test, no graphics required:

```bash
./build/beauty-face-region-test
```

Expected:

```text
OK: face region detection tests passed
```

The test builds a synthetic skin-mask grid (a face taller than it is wide, with eye/mouth holes and a skin-coloured wall patch), then checks:

1. The detector returns a valid box.
2. The box centre is close to the real face centre.
3. The box is not pulled off-centre by the wall/skin patch at the edge.
4. An empty or too-small grid returns `valid = false`.

## 3. Load + render integration test

The test creates a `beauty_filter`, attaches it to a source and renders through `gs_texrender`. If the effect has a syntax error or data files are missing, OBS may still return a source object with `obs_obj_invalid()` set to true; the test checks this and fails immediately. OBS 30.2 intentionally keeps an invalid source object so user settings are not lost, so a `NULL` pointer check alone is not enough.

### Linux

```bash
xvfb-run -a ./build/beauty-load-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter \
  /usr/lib/obs-plugins/image-source.so \
  /usr/share/obs/obs-plugins/image-source
```

Expected (values may vary by ±1–2 due to rounding):

```text
center pixel RGBA (original):   200 150 120 255
center pixel RGBA (processed):  202 151 123 255
center pixel RGBA (skin mask):  255 255 255 255
center pixel RGBA (opacity 0):  200 150 120 255
pattern eye/mouth luma before:  215 / 198
pattern eye/mouth luma after:   251 / 210
zone preview eye RGBA:          109 182 254 255
zone preview mouth RGBA:        245 251 98 255
HDR pixel before filter:        4.000 0.400 0.400 1.000
HDR pixel after filter:         4.098 0.420 0.445 1.000
scRGB pixel before filter:      4.000 0.400 0.400 1.000
scRGB pixel after filter:       4.375 0.475 0.568 1.000
OK: module loaded, filter created, effect compiled, filter rendered, mask/opacity/face zones/HDR verified
```

The test checks:

1. **Default filter** on skin tone `RGB(200,150,120)` stays close to the original (±8 due to default rosy/tone).
2. **Show skin mask** is fully white on a flat skin area.
3. **Opacity = 0** restores the original frame (±1 rounding).
4. Output alpha remains 255.
5. **Teeth/eye zones**: a synthetic pattern with a face, grey eyes and a grey mouth; `face_manual` is enabled with a box covering the face.
   - Mouth luma increases by at least 6 (actual 198 → 210).
   - Eye luma increases by at least 15 (actual 215 → 251).
6. **Zone preview**: eyes must be blue-ish (`B > R + 40`), mouth must be yellow (`R > 180` and `R > B + 30`).
7. **HDR Rec.709 extended**: the pattern draws a full frame of `(4.0, 0.4, 0.4)`; after filtering:
   - `R > 1.5` → the highlight was not clipped to SDR white.
   - `R > before + 0.05` → the source was actually processed, not bypassed.
8. **HDR scRGB**: the pattern reports `GS_CS_709_SCRGB` and draws the same `(4.0, 0.4, 0.4)`:
   - `R > 1.5` and `R > before + 0.05` as above.
   - `R < before * 1.5` → catches an incorrect HDR scale (missing/extra `hdr_scale`; the old bug produced ~15 instead of ~4.4).

If you get `0,0,0,0`, filter creation fails, or colours are far off, check the shader log below.

### Windows

```powershell
.\build\RelWithDebInfo\beauty-face-region-test.exe

# OBS 33+ layout (default on Windows)
.\build\RelWithDebInfo\beauty-load-test.exe `
  .\build\RelWithDebInfo\obs-beauty-filter.dll `
  .\build\rundir\plugins\obs-beauty-filter\data `
  "C:\Program Files\obs-studio\obs-plugins\64bit\image-source.dll" `
  "C:\Program Files\obs-studio\data\obs-plugins\image-source"

# Legacy layout (only when configured with -DBEAUTY_OBS33_PLUGIN_LAYOUT=OFF)
.\build\RelWithDebInfo\beauty-load-test.exe `
  .\build\RelWithDebInfo\obs-beauty-filter.dll `
  .\build\rundir\data\obs-plugins\obs-beauty-filter `
  "C:\Program Files\obs-studio\obs-plugins\64bit\image-source.dll" `
  "C:\Program Files\obs-studio\data\obs-plugins\image-source"
```

> The third argument is the **data** directory, not the DLL directory. With the
> OBS 33+ layout, data lives under `build\rundir\plugins\obs-beauty-filter\data`.
>
> The `image-source` path may differ between OBS installations. In OBS 33 this
> core module may live under the `core` directory of the OBS install or build
> tree; locate `image-source.dll` and its matching data directory. Without these
> arguments, the test only checks load + effect compilation.

## 4. Pixel-readback orientation note

The integration test reads pixels with `gs_stage_texture()` + `gs_stagesurface_map()`. On OpenGL/llvmpipe in the current CI environment, the first mapped row is the **top** row of the image (top-down), so `render_pixel()` uses `row = y` and the pattern draws with a top-based origin (`translate(x, y)`).

If a backend returns bottom-up rows (some drivers/backends may), the `pattern eye/mouth luma` values will be swapped and the zone checks will fail. In that case, verify the backend convention instead of loosening the test thresholds.

## 5. Checking the OBS log

Open **Help → Log Files → View Current Log** and look for:

- `Loading module: obs-beauty-filter` — module loaded.
- `Failed to load '...beauty_filter.effect'` — must **not** appear.
- `Compiler warnings/errors for ...beauty_filter.effect` — must **not** appear.
- `Source ID 'beauty_filter' not found` — must **not** appear after adding the filter.
- `[obs-beauty-filter] Failed to allocate GPU resources` — must **not** appear.

On first use, OBS compiles all five techniques (`MaskPass`, `MaskAnalysis`, `DrawFast`, `DrawBalanced`, `DrawHigh`); a failure in any technique makes the effect fail to load.

## 6. Manual test matrix

| # | Case | Expectation |
|---|---|---|
| 1 | Add filter to a 1080p camera | Filter appears; image is softly smoothed |
| 2 | Amount = 0, Opacity = 1 | Nearly original (tone adjustments = 0 when tone sliders are 0) |
| 3 | Opacity = 0 | Original frame exactly, even with Amount > 0 |
| 4 | Show skin mask | Skin white, eyes/lips/hair/background black |
| 5 | Show face zones | Face prior green, eyes blue, mouth yellow, aligned with the face |
| 6 | Split preview | Left original, right processed |
| 7 | Fast / Balanced / High | No shader errors; quality increases |
| 8 | Radius 0 | No smoothing (tone/clarity only) |
| 9 | Radius 24 @1080p | Clearly smooth, no crash, FPS drops |
| 10 | 720p / 1440p / 4K source | Equivalent radius, no errors |
| 11 | Source with alpha (PNG/scene) | Alpha edges preserved, no dark halo |
| 12 | HDR Rec.709 extended source | Filter processes it; highlights > 1 preserved; no bypass |
| 13 | HDR scRGB source | Filter processes it; highlights above SDR white preserved; no double scaling |
| 14 | Teeth = 0.5, Eyes = 0 | Teeth brighter; lips/skin not whitened |
| 15 | Eyes = 0.3 | Sclera brighter; skin around eyes not over-bright |
| 16 | Manual face box | Manual box replaces auto; zones follow it |
| 17 | Temporal = 0 / 0.6 / 0.9 | 0 = more flicker; 0.9 = smoother but may ghost on fast turns |
| 18 | Remove filter while rendering | No crash |
| 19 | Change settings continuously | No leak, no abnormal flicker |
| 20 | Stream/record for 10 minutes | No RAM/VRAM growth, no dropped frames |

## 7. Performance checks

1. Use OBS **Stats** (View → Stats) for FPS and dropped frames.
2. Compare the three Quality levels on the same scene.
3. Compare Face-aware On/Off (mask pass + small readback).
4. If GPU load > 90% or FPS drops:
   - lower Quality;
   - lower Radius;
   - disable Glow;
   - disable Face-aware if not using teeth/eye;
   - check other filters (denoise, sharpen).

The plugin never reads back a full frame. CPU cost is only the 64×N grid (at most 16 KB) every 6 frames when automatic face-aware detection is on.

## 8. Image-quality checks

- Capture before/after screenshots with the same lighting and frame.
- At 100% zoom, inspect eyes, eyebrows, lip line, sideburns and jawline.
- Inspect skin in the brightest and darkest areas to avoid losing detail.
- Check teeth/eye zones with a front-facing face, a 3/4 turn and a partially occluded face.
- For multiple skin tones, capture at least three light/dark levels and confirm both mask and face box.
- For HDR, compare highlights (lights, windows) before/after to confirm they are not clipped.

## 9. Windows-specific testing

- Windows 10/11, OBS 30.2+ (64-bit); confirm on OBS 33.0.1 or newer first.
- Intel iGPU, NVIDIA and AMD GPUs.
- Use **Dependencies** or **Process Monitor** if OBS does not see the DLL.
- OBS 33+ layout: the DLL and data must sit in the same plugin directory:
  `%ProgramData%\obs-studio\plugins\obs-beauty-filter\obs-beauty-filter.dll`
  and `%ProgramData%\obs-studio\plugins\obs-beauty-filter\data\`.
  For portable OBS, replace `%ProgramData%\obs-studio\plugins` with
  `<OBS-portable-root>\plugins`.
- Legacy layout (OBS 32 and older): DLL in `obs-plugins\64bit`, data in
  `data\obs-plugins\obs-beauty-filter`. OBS 33 still loads it but flags it as
  Legacy, and OBS 34 will stop loading it.
- If OBS 33 starts in **Safe Mode**, third-party plugins are not loaded; leave
  Safe Mode before testing.
- Run both `beauty-face-region-test.exe` and `beauty-load-test.exe`; verify both HDR Rec.709 extended and scRGB on real hardware.
- The headless test now covers scRGB; 16F readback can still differ between D3D11 and OpenGL, so confirm with a real HDR source/canvas.

## 10. Suggested CI

A Linux job can run:

```yaml
- cmake -S . -B build -DBEAUTY_BUILD_TESTS=ON
- cmake --build build -j
- ./build/beauty-face-region-test
- xvfb-run -a ./build/beauty-load-test ...
```

Windows CI needs OBS deps + the libobs package; follow the same approach as `obs-plugintemplate`. At least one Windows/D3D11 job should run `beauty-load-test` before release.
