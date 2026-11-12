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

### Sanitizers

AddressSanitizer and UndefinedBehaviorSanitizer are opt-in:

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

Use `-DBEAUTY_SANITIZE=undefined` when the ASan runtime cannot map its shadow
memory (for example inside some containers). On musl/Alpine, configure with
`CC=clang`; Alpine's GCC does not ship `libasan`/`libubsan`. Leak detection is
disabled because libobs and Mesa keep process-lifetime allocations that the
plugin does not own.

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

## 3. Preset unit test

Pure CPU test, no graphics required:

```bash
./build/beauty-preset-test
```

Expected:

```text
OK: built-in and custom presets, lookup, apply, defaults and NULL handling verified
```

It checks the preset count and unique IDs, value ranges, quality strings, `find()` behaviour for `custom`/unknown ids, that `apply()` writes every tuning key without overwriting the preset selection, that the `default` preset stays in sync with `beauty_defaults()`, and that `NULL` arguments are ignored.

The custom-preset part writes a real JSON file through
`OBS_BEAUTY_CUSTOM_PRESET_PATH`, reloads it from disk, and verifies trimming,
case-insensitive overwrite/delete, rejection of built-in and reserved names,
control-character rejection, name-length limits, and missing-key fallback to
the Default preset. The three temporary files (`json`, `.tmp`, `.bak`) are
removed at the end.

## 4. Load + render integration test

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

The test checks:

1. **Default filter** on skin tone `RGB(200,150,120)` stays close to the original (±8 due to default rosy/tone).
2. **Show skin mask** is fully white on a flat skin area.
3. **Opacity = 0** restores the original frame (±1 rounding).
4. Output alpha remains 255.
5. **Teeth/eye zones**: a synthetic pattern with a face, grey eyes and a grey mouth; `face_manual` is enabled with a box covering the face.
   - Mouth luma increases by at least 6 (actual 198 → 204).
   - Eye luma increases by at least 15 (actual 215 → 243).
6. **Zone preview**: eyes must be blue-ish (`B > R + 40`), mouth must be yellow (`R > 180` and `R > B + 30`).
7. **Frequency separation**: a synthetic skin pattern with 10 px alternating stripes (two close skin tones) is rendered twice.
   - With `Amount=1`, `Radius=24`, `Detail=0`, `skin_detect=on`, the stripe contrast must fall to at most 55% of its original value (actual 10 → 1–2).
   - With `Detail=0.9`, the same filter must keep at least 3 more contrast levels than `Detail=0` (actual 8), proving the detail control and not a plain blur.
8. **HDR Rec.709 extended**: the pattern draws a full frame of `(4.0, 0.4, 0.4)`; after filtering:
   - `R > 1.5` → the highlight was not clipped to SDR white.
   - `R > before + 0.05` → the source was actually processed, not bypassed.
9. **HDR scRGB**: the pattern reports `GS_CS_709_SCRGB` and draws the same `(4.0, 0.4, 0.4)`:
   - `R > 1.5` and `R > before + 0.05` as above.
   - `R < before * 1.5` → catches an incorrect HDR scale (missing/extra `hdr_scale`; the old bug produced ~15 instead of ~4.4).

If you get `0,0,0,0`, filter creation fails, or colours are far off, check the shader log below.

### Windows

```powershell
.\build\RelWithDebInfo\beauty-face-region-test.exe
.\build\RelWithDebInfo\beauty-preset-test.exe

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

### Render matrix / golden image

`beauty-render-matrix-test` renders every built-in preset at 360p, 720p, 1080p
and 1440p, then runs a 16F pass at 720p. It checks that output alpha stays
opaque, that every preset changes the frame, and that 16F values are finite,
non-negative and keep highlights above SDR white.

```bash
# Invariants only (Linux and Windows)
xvfb-run -a ./build/beauty-render-matrix-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter

# Compare the sampled golden image (Linux CI)
BEAUTY_MATRIX_GOLDEN=tests/golden/render-matrix.txt \
xvfb-run -a ./build/beauty-render-matrix-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter

# Regenerate the golden image after an intentional visual change
BEAUTY_MATRIX_WRITE_GOLDEN=tests/golden/render-matrix.txt \
xvfb-run -a ./build/beauty-render-matrix-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter

# Opt-in 4K pass and PPM dumps for visual inspection
BEAUTY_MATRIX_4K=1 BEAUTY_MATRIX_DUMP_DIR=/tmp/beauty-frames \
xvfb-run -a ./build/beauty-render-matrix-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter
```

The golden file stores the average RGB of an 8×6 grid, not a byte-exact frame,
so it tolerates GPU rounding while still catching shader/preset regressions.
`BEAUTY_MATRIX_TOLERANCE` sets the maximum per-channel difference (default 20).
Windows CI runs the invariants without the golden comparison because D3D11 and
OpenGL round differently.

## 5. Pixel-readback orientation note

The integration test reads pixels with `gs_stage_texture()` + `gs_stagesurface_map()`. On OpenGL/llvmpipe in the current CI environment, the first mapped row is the **top** row of the image (top-down), so `render_pixel()` uses `row = y` and the pattern draws with a top-based origin (`translate(x, y)`).

If a backend returns bottom-up rows (some drivers/backends may), the `pattern eye/mouth luma` values will be swapped and the zone checks will fail. In that case, verify the backend convention instead of loosening the test thresholds.

## 6. Checking the OBS log

Open **Help → Log Files → View Current Log** and look for:

- `Loading module: obs-beauty-filter` — module loaded.
- `Failed to load '...beauty_filter.effect'` — must **not** appear.
- `Compiler warnings/errors for ...beauty_filter.effect` — must **not** appear.
- `Source ID 'beauty_filter' not found` — must **not** appear after adding the filter.
- `[obs-beauty-filter] Failed to allocate GPU resources` — must **not** appear
  during normal operation. When it does, the filter shows a red error message
  and passes the source through.
- `[obs-beauty-filter] Could not read/write custom presets` — appears only when
  `custom-presets.json` is unreadable or the config directory is not writable.
  Check the path and file permissions; the filter keeps working without the
  custom list.

On first use, OBS compiles all six techniques (`MaskPass`, `MaskAnalysis`, `BlurBase`, `DrawFast`, `DrawBalanced`, `DrawHigh`); a failure in any technique makes the effect fail to load.

## 7. Manual test matrix

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
| 21 | Textured skin, Detail 0 → 1 | Detail 0 smooths; Detail 1 brings pores/fine lines back |
| 22 | Smooth gradient, 8-bit output | Anti-banding on: no visible bands; off: bands return |
| 23 | High-end 1440p/4K camera, Quality High | Skin smooth but detailed, no halo or plastic look |
| 24 | Save a custom preset, restart OBS | Preset appears as `Custom: <name>` and restores every control |
| 25 | Save over an existing name with different case | One entry remains, values are updated |
| 26 | Delete the selected custom preset | Entry disappears and the selection falls back to `Custom` |
| 27 | Reset to defaults | Every control returns to the Default preset |
| 28 | Rename `data/beauty_filter.effect` temporarily | Filter passes the source through, shows the GPU error message, no black frame |
| 29 | Hand-edit `custom-presets.json` with a missing key | Preset loads, missing value falls back to Default |
| 30 | Run the `.exe` installer on a clean OBS 33 machine | Setup writes the DLL and `data\` under `%ProgramData%\obs-studio\plugins\obs-beauty-filter`, OBS lists **Beauty Filter**, and uninstalling removes only the plugin |
| 31 | Drag the Amount slider and release | The value follows the pointer and does not snap back; the preset switches to `Custom` |
| 32 | Light balance 0.35, cover the key light / turn it off | The image brightens gradually without pumping; turning the light back on restores the previous level |
| 33 | Light balance 0 (off) | The image matches the filter with the feature disabled, independent of scene content |

## 8. Benchmark guide — Intel i5-1235U

This is the reference low-power machine for the performance targets: Intel
Core i5-1235U (2 P-cores + 8 E-cores), Iris Xe 80 EU, 16 GB **dual-channel**
DDR4/DDR5, Windows 11, OBS 30.2 or newer. Single-channel memory roughly halves
iGPU bandwidth and is not a valid comparison.

### Method

1. Use a real camera or a 30 fps / 60 fps test source at the target resolution.
   Do not benchmark the filter on a paused or still image.
2. Run OBS in **Stats** (View → Stats) for at least 5 minutes with the filter
   **removed**; record *average time to render frame*, *frames missed* and GPU
   utilisation.
3. Add the filter with the preset and Quality level under test. Run another 5
   minutes and record the same numbers.
4. Filter overhead = filtered frame time − unfiltered frame time. The absolute
   frame time matters more than the overhead because OBS also renders the scene
   and encodes.
5. Verify: no missed frames, GPU below 85–90 %, no memory growth, and the UI
   stays responsive.
6. Repeat for each preset you actually use. Presets differ mostly in Radius,
   Detail, Glow and Quality, so `Studio` and `Soft` are the heaviest.

Do not use a synchronous `gs_timer` query for production measurements: it
stalls the GPU pipeline and inflates the result. OBS Stats already reports the
frame time without that stall.

### Recommended thresholds

| Source | FPS | Quality | Preset | Max filter overhead | Max total frame time | Pass criteria |
|---|---:|---|---|---:|---:|---|
| 1080p | 30 | Balanced | Default / Natural | 3.0 ms | 20 ms | 0 missed frames in 10 min |
| 1080p | 60 | Balanced | Default / Natural | 4.0 ms | 12 ms | 0 missed frames, GPU < 90 % |
| 1080p | 60 | High | Cinema / Studio | 6.0 ms | 12 ms | 0 missed frames; step down if missed |
| 1440p | 30 | High | Cinema | 6.0 ms | 20 ms | 0 missed frames |
| 1440p | 60 | Balanced | Cinema | 8.0 ms | 12 ms | 0 missed frames; High is usually too slow |
| 4K | 30 | Balanced | Cinema | 12 ms | 25 ms | 0 missed frames; use Fast if needed |
| 4K | 60 | Fast | any | 6.0 ms | 16 ms | Not recommended on Iris Xe |

These are guidance values, not hard guarantees: thermal limits, shared memory
bandwidth and other OBS sources change the result. A machine that cannot meet
the threshold should move **Fast** before reducing image quality below what the
scene needs.

### If the benchmark fails

1. Lower **Quality** one step (High → Balanced → Fast).
2. Lower **Radius** (for example 10 → 6) and **Glow**.
3. Disable **Face-aware zones** when you do not use teeth/eye controls.
4. Remove stacked filters (denoise/sharpen) or move them after the beauty
   filter only if the result still looks correct.
5. Check that the laptop is on AC power, in performance mode, and that the
   camera is not forcing a higher resolution than the canvas.

Record the result in your issue report with the preset, Quality, resolution,
frame times and OBS version. See the compatibility matrix in
[COMPATIBILITY.en.md](COMPATIBILITY.en.md).

## 9. Performance checks

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

## 10. Image-quality checks

- Capture before/after screenshots with the same lighting and frame.
- At 100% zoom, inspect eyes, eyebrows, lip line, sideburns and jawline.
- Inspect skin in the brightest and darkest areas to avoid losing detail.
- Check teeth/eye zones with a front-facing face, a 3/4 turn and a partially occluded face.
- For multiple skin tones, capture at least three light/dark levels and confirm both mask and face box.
- For HDR, compare highlights (lights, windows) before/after to confirm they are not clipped.

## 11. Windows-specific testing

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
- Run `beauty-face-region-test.exe`, `beauty-preset-test.exe` and `beauty-load-test.exe`; verify both HDR Rec.709 extended and scRGB on real hardware.
- The headless test now covers scRGB; 16F readback can still differ between D3D11 and OpenGL, so confirm with a real HDR source/canvas.
- When editing the effect, avoid single-scalar vector constructors such as `float3(x)`: libobs-d3d11's FXC rejects them with `X3014: incorrect number of arguments to numeric-type constructor`. Write `float3(x, x, x)` instead. The Linux/OpenGL compiler accepts both, so only the Windows CI catches this.

### Installer smoke test

Windows CI compiles `installer/obs-beauty-filter.iss` with Inno Setup 6 after
packaging and then installs the setup silently into a temporary directory:

```powershell
.\obs-beauty-filter-1.6.0-windows-x64-setup.exe /VERYSILENT /SUPPRESSMSGBOXES `
  /NORESTART /SP- /DIR=C:\temp\beauty-installer-test /LOG=install.log
```

It verifies `obs-beauty-filter.dll`, `data\beauty_filter.effect`,
`data\locale\en-US.ini`, `data\locale\vi-VN.ini` and `unins000.exe`, runs the
uninstaller silently, and confirms the DLL is gone.

To reproduce locally without touching the real OBS installation, pass `/DIR`
pointing at an empty directory. To exercise portable detection, use a real
portable OBS tree (or a temporary tree containing `bin\64bit\obs64.exe` and
`portable_mode.txt`) and pass `/OBSDIR=<tree>`; setup should install into
`<tree>\plugins\obs-beauty-filter`. Running setup without either switch on a
machine without OBS installs to `%ProgramData%\obs-studio\plugins`.

## 12. Suggested CI

A Linux job can run:

```yaml
- cmake -S . -B build -DBEAUTY_BUILD_TESTS=ON
- cmake --build build -j
- ./build/beauty-face-region-test
- ./build/beauty-preset-test
- xvfb-run -a ./build/beauty-load-test ...
```

Windows CI needs OBS deps + the libobs package; follow the same approach as
`obs-plugintemplate`. This repository's CI builds OBS 33.0.1 from source with
`obs-deps` and runs `beauty-load-test` on D3D11 (module load + effect
compilation) before packaging. The full render/mask/HDR matrix still runs on
Linux/OpenGL; keep the manual matrix in section 7 for GPU-specific validation.
