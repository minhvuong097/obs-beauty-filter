# OBS Beauty Filter

**English | [Tiếng Việt](README.md)**

A GPU beauty filter for **OBS Studio** on Windows and Linux (macOS can be built from source but is not tested). Version **1.2.0** runs entirely on the GPU, plugs into the standard OBS filter pipeline, uses no AI model, never reads a full-resolution frame back to the CPU, and has no third-party runtime dependency.

Goal: natural-looking skin for livestreams/webcams **and high-resolution cameras** while preserving eyes, eyebrows, hair, pores and the face outline — with correct HDR handling.

## Features

- **Frequency separation for skin**: a low-resolution bilateral base (1/2–1/4 res, 25-tap Vogel disk) with full-resolution detail reconstruction — smooths broad skin blotches while keeping texture and reducing halos at 1440p/4K.
- **Edge-preserving skin smoothing**: 9/13/25-tap bilateral filter that softens pores and minor blemishes without erasing edges.
- **Layered skin mask**: YCbCr skin detection plus **neighbourhood coherence**, **gradient/texture rejection** (hair, wood grain) and **temporal stabilisation** to reduce flicker.
- **Heuristic face region**: estimates a face box from the largest connected skin component on a tiny 64×N grid, with temporal smoothing and a **manual face box** fallback. No ONNX/MediaPipe required.
- **Face prior**: an ellipse over the face and neck rejects most skin-coloured walls, furniture and clothing.
- **Teeth whitening**: brightens neutral, low-chroma, bright pixels inside the mouth ellipse.
- **Eye brightening**: lifts the sclera and gently raises the iris/pupil inside two eye ellipses.
- **Detail reconstruction**: retains luma texture (pores) more than chroma (blotches) and adds clarity outside skin.
- **Complexion controls**: exposure-like brightness, luma-preserving warmth, saturation and rosy tint, all skin-weighted.
- **Soft glow** only on highlights, useful with ring lights and clean shadows.
- **Anti-banding**: triangular dithering for 8-bit output prevents banding in smooth skin gradients.
- **HDR-safe**: `GS_CS_709_EXTENDED`, `GS_CS_709_SCRGB` and `GS_CS_SRGB_16F` are processed in linear light; highlights above SDR white are preserved (no bypass, no clipping).
- **Previews**: skin mask, split view and face/eye/mouth zones.
- **Three performance levels**: Fast / Balanced / High.
- **Alpha support**, SDR 8-bit and SDR 16-bit.
- **Bilingual UI**: English and Vietnamese.

## Project layout

```
obs-beauty-filter/
├── CMakeLists.txt
├── src/
│   ├── beauty-filter.c      # OBS filter + properties + multi-pass renderer
│   ├── beauty-filter.h
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
│   └── face-region-test.c   # face-detector unit test
├── docs/
│   ├── ARCHITECTURE.md      # + ARCHITECTURE.en.md
│   ├── TUNING.md            # + TUNING.en.md
│   └── TESTING.md           # + TESTING.en.md
└── scripts/
    ├── build-linux.sh
    └── build-windows.ps1
```

## Windows installation

### Option A — prebuilt binary (if available)

**OBS Studio 33+ — new layout (recommended):**

1. Close OBS Studio.
2. Create this directory:
   `C:\ProgramData\obs-studio\plugins\obs-beauty-filter\`
3. Copy `obs-beauty-filter.dll` into it.
4. Copy the whole `data` directory into:
   `C:\ProgramData\obs-studio\plugins\obs-beauty-filter\data\`
   You should end up with:
   - `...\obs-beauty-filter\obs-beauty-filter.dll`
   - `...\obs-beauty-filter\data\beauty_filter.effect`
   - `...\obs-beauty-filter\data\locale\en-US.ini`
5. Open OBS → select the camera source → **Filters** → **+** → **Beauty Filter**.

> `%ProgramData%` is usually hidden. For portable OBS, use
> `<OBS-portable-root>\plugins\obs-beauty-filter\` instead of
> `C:\ProgramData\obs-studio\plugins\obs-beauty-filter\`.

**OBS 32 and older — legacy layout:**

1. Copy `obs-beauty-filter.dll` to:
   `C:\Program Files\obs-studio\obs-plugins\64bit\`
2. Copy the contents of `data` to:
   `C:\Program Files\obs-studio\data\obs-plugins\obs-beauty-filter\`

> OBS 33 still loads the legacy layout but flags it as **Legacy** in Plugin
> Manager; OBS 34 will stop loading it. Prefer the new layout above.
>
> If OBS 33 starts in **Safe Mode**, third-party plugins are not loaded. Leave
> Safe Mode and restart OBS.

### Option B — build from source (recommended for development)

**In-tree build** (easiest if you already build OBS from source):

```powershell
# Copy this directory to obs-studio/plugins/obs-beauty-filter
# Add this line to obs-studio/plugins/CMakeLists.txt:
#   add_core_module(obs-beauty-filter)
```

Then build OBS as usual (CMake preset + Visual Studio 2026, or the toolchain
required by your OBS 33 build). The plugin and data are copied into the OBS
build tree using OBS' core-module layout. This is for local development only:
in OBS 33 the `core` directory is reserved for official plugins. Release builds
for users should use the standalone build and install into the `plugins`
directory.

**Standalone build** (requires an OBS SDK/build tree that provides `libobsConfig.cmake`):

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_PREFIX_PATH="C:\obs-studio\build_x64;C:\obs-deps\x64"
cmake --build build --config RelWithDebInfo
cmake --install build --config RelWithDebInfo `
  --prefix "$env:ProgramData\obs-studio\plugins"
```

If you use Visual Studio 2022, replace the generator with
`"Visual Studio 17 2022"`. With the new layout, `--prefix` must point to the
**plugins directory**, not the OBS install root:

- Normal OBS: `%ProgramData%\obs-studio\plugins`
- Portable OBS: `<OBS-portable-root>\plugins`

To package for the old layout (OBS 32 and older), add
`-DBEAUTY_OBS33_PLUGIN_LAYOUT=OFF` and use the prefix `C:\Program Files\obs-studio`.

Or use the script:

```powershell
# Build + install into the OBS 33+ layout
.\scripts\build-windows.ps1 -ObsBuild "C:\obs-studio\build_x64" `
  -ObsSdk "C:\obs-deps\x64" `
  -InstallPrefix "$env:ProgramData\obs-studio\plugins"

# Build + create a distribution zip (zip root is obs-beauty-filter\)
.\scripts\build-windows.ps1 -ObsBuild "C:\obs-studio\build_x64" `
  -ObsSdk "C:\obs-deps\x64" -Package

# Legacy layout for OBS 32 and older
.\scripts\build-windows.ps1 -Layout legacy `
  -InstallPrefix "C:\Program Files\obs-studio"
```

> The script automatically picks the Visual Studio 2026 generator when both
> CMake and a local VS 2026 installation are available; otherwise it falls back
> to Visual Studio 2022. The zip is written to `dist\`; extract it into the
> matching `plugins` directory above (or the portable OBS `...\plugins\`
> directory).

> If you only have an installed OBS without the OBS SDK/source, use the prebuilt binary from **Option A**. Building requires OBS source (in-tree) or an OBS SDK/CMake package (standalone).

### Linux

```bash
sudo apt install cmake build-essential libobs-dev   # or the equivalent package
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

- **Debian/Ubuntu:** add `-DCMAKE_INSTALL_LIBDIR="lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)"`
  so the plugin lands in `/usr/lib/<triplet>/obs-plugins`.
- **Fedora/Arch:** check OBS' `obs-plugins` directory and set `CMAKE_INSTALL_LIBDIR`
  accordingly if needed.
- Avoid the default `/usr/local` prefix unless your OBS searches that path.

Alternatively run `./scripts/build-linux.sh` then `sudo cmake --install build` (default prefix `/usr`;
set `INSTALL_LIBDIR=...` for Debian/Ubuntu multiarch).

## Quick start

1. Add **Beauty Filter** to the camera source (or to a scene containing the camera).
2. Start with the defaults:
   - **Amount** `0.35`
   - **Radius** `6`
   - **Detail preservation** `0.35`
   - **Skin detection** on, **Skin sensitivity** `0.50`
   - **Face-aware zones** on, **Temporal mask stability** `0.60`
   - **Rosy tint** `0.15`
   - **Teeth whitening / Eye brightening** `0` (enable when needed)
3. Enable **Show skin mask** to inspect the skin mask. If skin-coloured walls/wood also turn white, keep **Face-aware zones** on and lower **Skin sensitivity**.
4. Enable **Show face zones** to inspect the face box, eyes and mouth. If the box is wrong, enable **Manual face box** and adjust `Face center X/Y`, `Face width/height`.
5. Raise **Teeth whitening** to `0.2–0.5` and **Eye brightening** to `0.1–0.3`. If surrounding skin brightens too, reduce these values.
6. Enable **Split preview** to compare the left half (original) with the right half (processed), then turn it off.
7. If skin looks flat, raise **Detail preservation**, lower **Amount** or **Radius**.
8. If the mask flickers during head motion, raise **Temporal mask stability** (`0.6–0.75`). If you see ghosting, lower it to `0.3–0.5`.

## Parameters

| Parameter | Default | Range | Meaning |
|---|---:|---:|---|
| Amount | 0.35 | 0–1 | Skin smoothing strength |
| Radius (px @1080p) | 6 | 0–24 | Sampling radius, scaled by frame height |
| Detail preservation | 0.35 | 0–1 | Retains skin texture/detail |
| Skin detection | On | On/Off | Process skin only |
| Skin sensitivity | 0.50 | 0–1 | Widens/tightens the skin mask |
| Show skin mask | Off | On/Off | White/black mask preview for tuning |
| Brightness | 0 | -50..50 | Skin brightness (mask-weighted) |
| Warmth | 0 | -50..50 | Warm/cool skin tint |
| Saturation | 0 | -50..50 | Skin colour saturation |
| Rosy tint | 0.15 | 0–1 | Natural rosy tint |
| Clarity | 0.10 | 0–1 | Sharpens non-skin areas |
| Soft glow | 0.05 | 0–1 | Soft skin glow |
| Face-aware zones | On | On/Off | Enables face prior + teeth/eye zones |
| Temporal mask stability | 0.60 | 0–1 | Blends mask with history; higher = more stable, less flicker |
| Teeth whitening | 0 | 0–1 | Whitens teeth inside the mouth zone |
| Eye brightening | 0 | 0–1 | Brightens eyes inside the eye zones |
| Show face zones | Off | On/Off | Shows face prior (green), eyes (blue), mouth (yellow) |
| Manual face box | Off | On/Off | Use the manual box instead of auto detection |
| Face center X / Y | 0.50 / 0.45 | 0–1 | Face centre in normalised coordinates |
| Face width / height | 0.32 / 0.42 | 0.05–1 | **Full** face-box size, normalised |
| Quality | Balanced | Fast/Balanced/High | 1/4, 1/3 or 1/2 resolution base; High recommended for 1440p/4K |
| Anti-banding | On | On/Off | Triangular dither for 8-bit output |
| Split preview | Off | On/Off | Left = original, right = processed |
| Overall opacity | 1.0 | 0–1 | Global mix with the original frame |

Note: `Face width/height` are full dimensions, not half-extents. For example a face covering 32% of the frame width and 42% of its height.

### Situation-based suggestions

- **1080p webcam, indoor lighting**: Amount `0.35–0.5`, Radius `5–8`, Detail `0.3–0.5`, Clarity `0.05–0.15`.
- **High-end 1440p/4K camera**: Quality `High`, Amount `0.20–0.35`, Radius `6–12`, Detail `0.45–0.65`, Clarity `0.05–0.10`, Skin sensitivity `0.45–0.55`. The base pass keeps full-resolution detail, so skin looks smooth rather than flat.
- **Close-up / blemished skin**: Amount `0.55–0.7`, Radius `8–12`, Detail `0.25`, Quality `High`, Teeth `0.3–0.5`, Eyes `0.2–0.4`.
- **Men / beards**: Amount `0.2–0.3`, Detail `0.5–0.7`, Radius `4–6`, Clarity `0.1`; lower Teeth/Eyes.
- **Deep skin tones**: Sensitivity `0.6–0.75`, Amount `0.25–0.4`; always verify with **Show skin mask**.
- **Low light / noise**: Beauty Filter does not denoise. Add **Denoise** before it, then Amount `0.2–0.35`, Glow `0.1`.
- **4K / many stacked filters**: Quality `High` on a discrete GPU with headroom; if frame rate drops, move to `Balanced` (1/3 base) before `Fast`. Radius scales automatically with resolution.
- **HDR source (Rec.709 extended / scRGB / 16F)**: use the same controls; the filter processes linear light and preserves highlights. Re-check Amount/Glow because HDR has a wider brightness range.

See also [docs/TUNING.en.md](docs/TUNING.en.md).

## Technical architecture

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
   │      ├─ neighbourhood coherence (8 samples)
   │      ├─ gradient/texture guard
   │      ├─ temporal blend with history
   │      └─ face prior (face + neck ellipse)
   │      Output: R = raw mask, G = stabilised mask
   │
   ├─► MaskAnalysis (64×N)  ──► small CPU readback (every 6 frames)
   │                              └─► face-region.c: connected component
   │                                  + upper band → face box
   │
   ├─► BlurBase (1/2–1/4 res, RGBA16F)
   │      └─ 25-tap Vogel disk bilateral → low-frequency base
   │
   └─► DrawFast / DrawBalanced / DrawHigh
          ├─ frequency separation: base + reconstructed detail
          ├─ detail reconstruction + clarity
          ├─ complexion + glow (HDR-safe)
          ├─ teeth/eye zones (only with a valid face box)
          ├─ anti-banding dither
          ├─ opacity / split / previews
          └─ HDR scale restore
   │
   ▼
Output (alpha preserved, values > SDR white preserved on HDR)
```

- **No longer a single pass**: the mask pass runs at half resolution, the analysis pass at 64×N, the base pass at 1/2–1/4 resolution, and the main pass at full resolution.
- **Frequency separation**: the base pass blurs broad skin areas at reduced resolution with a 25-tap Vogel disk; the final pass restores full-resolution detail, weighting luma more than chroma. This preserves pores, reduces halos and avoids sparse kernels at 1440p/4K.
- **No full-frame readback**: only the 64×N grid (at most 64×64 RGBA = 16 KB) is read back, and only when `Face-aware zones` is on, automatic detection is used, once every 6 frames.
- **Ping-pong mask textures** provide history for temporal stabilisation without per-frame allocation.
- **The base is RGBA16F** even for SDR, avoiding linear 8-bit quantisation in shadows before the sRGB encode.
- Radius is normalised to frame height (1080p reference), so one preset behaves similarly at 720p/1080p/4K.
- **HDR**: `GS_CS_709_EXTENDED` and `GS_CS_SRGB_16F` use `hdr_scale = 1.0`; `GS_CS_709_SCRGB` uses `80 / obs_get_video_sdr_white_level()`. The frame is captured directly in the **output space** so OBS performs any source conversion/tonemapping; the shader brings samples into SDR-relative linear exactly once, does not `saturate` HDR output, and divides by the scale before writing.
- Maths and design rationale: [docs/ARCHITECTURE.en.md](docs/ARCHITECTURE.en.md).

## Performance

The dominant cost is the full-resolution fragment shader, plus a half-resolution mask pass, a 1/2–1/4-resolution base pass (25 taps) and a tiny readback:

| Quality | Base | Notes |
|---|---|---|
| Fast | 1/4 res | iGPU, 4K, or several stacked filters |
| Balanced | 1/3 res | Default for 1080p60 |
| High | 1/2 res | Close-ups, discrete GPU, 1440p/4K — recommended for high-end cameras |

- `MaskPass` runs at half resolution with 8 coherence + 4 gradient samples; much cheaper than full resolution.
- `MaskAnalysis` runs only every 6 frames when automatic face-aware detection is on.
- `BlurBase` is always cheaper than a full-resolution bilateral at the same radius because it touches 1/4–1/16 of the pixels.
- Disable **Face-aware zones** when you do not use teeth/eye controls to skip the mask pass when `Skin detection` is also off.
- If frame rate drops: lower Quality first, then Radius, then disable Glow.

## Known limitations

- **Face detection is heuristic**, not ML landmark/segmentation. It finds the largest skin component and derives the face from its upper band; it can fail with multiple people, occluded faces, hats, strong lighting or skin-coloured backgrounds. Use **Show face zones** and **Manual face box**.
- **Teeth/eye zones are estimated ellipses**, not per-tooth/per-eyelid landmarks. Best results with a front-facing face that fills a good part of the frame.
- The skin mask is still colour-based; coherence + face prior reduce but do not eliminate skin-coloured wood/walls/clothing.
- No noise reduction.
- HDR is processed but not tone-mapped; the filter preserves the source dynamic range.
- Results depend on skin tone and lighting; test with multiple skin tones before production use.
- The Windows D3D11 path must be built and tested on a Windows machine before a production release.

## Roadmap

1. Built-in presets (Natural, Studio, Soft, Strong, HDR) and quick reset.
2. Compute shader (D3D11/OpenGL) for large radii at 4K.
3. Measure and display filter GPU time.
4. Optional lightweight ML landmarks/segmentation (ONNX/MediaPipe) for multi-face and occlusion.
5. Landmark-driven automatic face boxes instead of the upper-band heuristic.

## Testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBEAUTY_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"

# Face-detector unit test
./build/beauty-face-region-test

# Load + render + mask/face/HDR integration test
xvfb-run -a ./build/beauty-load-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter \
  /usr/lib/obs-plugins/image-source.so \
  /usr/share/obs/obs-plugins/image-source
```

Expected final line:

```text
OK: module loaded, filter created, effect compiled, filter rendered, mask/opacity/face zones/texture/HDR verified
```

The integration test covers: default filter output, skin mask, opacity 0, teeth/eye zones (with a manual face box), zone preview colours, frequency separation on a textured skin pattern (contrast drops at low Detail and returns at high Detail), Rec.709 extended HDR and scRGB HDR (values above 1.0 are preserved, actually processed, and not double-scaled). No shader errors are allowed. See [docs/TESTING.en.md](docs/TESTING.en.md) for the full test matrix.

> **Test status:** the project builds and runs automated tests with OBS 30.2.3 on Linux/OpenGL (Mesa llvmpipe). The source/API has been reviewed against the OBS 33.0.1 headers, and the CMake build plus Windows script support the new OBS 33 plugin layout. The shader uses standard OBS effect syntax and compiles on OpenGL. The D3D11 path and the `%ProgramData%\obs-studio\plugins` layout still need to be built and tested on Windows with OBS 33.0.1 before a production release.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).

This is an independent project and is not part of the OBS Project. OBS Studio is software by the OBS Project and its contributors.
