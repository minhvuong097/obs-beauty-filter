# Compatibility matrix

**English | [Tiếng Việt](COMPATIBILITY.md)**

This page lists what is covered by CI, what has been tested manually, and what
is buildable but untested. "CI" means the full automated suite runs on every
push and tag.

## Tested combinations

| Platform | OBS Studio | Graphics backend | Status | Notes |
|---|---|---|---|---|
| Linux x86_64, Alpine 3.21 | 30.2.3 | OpenGL / Mesa llvmpipe (software) | **CI, full suite** | Render matrix + golden image, load/render/HDR test, ASan/UBSan |
| Windows x64 (GitHub `windows-2022`) | 33.0.1 (built from source) | D3D11 (WARP/runner GPU) | **CI, full suite** | Effect compilation, load/render test, render-matrix invariants, packaging |
| Windows 10/11 x64 | 33.0.1 | D3D11, real GPU | Manually tested | Prebuilt package, OBS 33+ plugin layout |
| Windows 10/11 x64 | 30.2 – 32.x | D3D11, real GPU | Buildable / legacy layout | OBS 33 still loads the legacy layout but marks it Legacy |
| Linux x86_64 | 30.2 – 33.x | OpenGL, real GPU | Buildable | Package path depends on the distribution |
| macOS | 30.2+ | OpenGL (deprecated by Apple) | Buildable, untested | No prebuilt package |

## OBS Studio versions

| OBS | Status | Plugin layout |
|---|---|---|
| 30.2 LTS | Supported, CI | Legacy on Windows (`obs-plugins/64bit` + `data/obs-plugins/...`) |
| 31.x | Supported | Legacy on Windows |
| 32.x | Supported | Legacy on Windows |
| 33.x | Supported, CI | New layout (`plugins/obs-beauty-filter/...`) is the default |
| 34+ | Not released yet | New layout expected; verify before release |

The plugin uses the stable `libobs` filter and graphics APIs. A new OBS major
version can still change shader compilation, the plugin directory layout or the
colour-space negotiation; re-run the test suite and the manual matrix before
claiming support.

## Build requirements

| Requirement | Minimum |
|---|---|
| CMake | 3.28 |
| C compiler | C99 (GCC, Clang or MSVC 2022) |
| OBS development files | 30.2 (`libobsConfig.cmake` for standalone builds) |
| Linux runtime | OpenGL 3.3-capable driver, X11 or XWayland for the headless test |
| Windows runtime | D3D11-capable GPU, OBS 30.2+ 64-bit |
| Sanitizers (optional) | Clang + compiler-rt on musl/Alpine; GCC with libasan on glibc |

## Feature support

| Feature | Status |
|---|---|
| SDR 8-bit (`GS_CS_SRGB`) | Supported, tested |
| SDR 16-bit float (`GS_CS_SRGB_16F`) | Supported, 16F render test |
| HDR Rec.709 extended (`GS_CS_709_EXTENDED`) | Supported, tested |
| HDR scRGB (`GS_CS_709_SCRGB`) | Supported, tested |
| Alpha | Preserved, tested |
| Dolby Vision / other HDR transport | Not supported |
| 10-bit integer sources | Converted by OBS before the filter; not a native path |
| Multi-face tracking | Not supported (heuristic single face box) |
| Compute shaders | Not used; works on OpenGL 3.3 / D3D11 feature level 10+ |

## Quality levels

| Quality | Base pass | Intended hardware |
|---|---|---|
| Fast | 1/4 resolution | Intel/AMD integrated GPU, 4K, stacked filters |
| Balanced | 1/3 resolution | Default for 1080p60 |
| High | 1/2 resolution | Discrete GPU, 1440p/4K, close-ups |

## Known gaps

- The golden image is generated on Linux/llvmpipe. Windows CI runs the same
  resolution/alpha/16F invariants but does not compare pixels, because D3D11
  and OpenGL round differently.
- Real-hardware GPU validation (Intel Iris Xe, NVIDIA, AMD) is manual. See the
  benchmark guide in [TESTING.en.md](TESTING.en.md).
- The Windows D3D11 path is compiled and smoke-tested in CI; visual quality on
  real cameras still needs a manual review.
