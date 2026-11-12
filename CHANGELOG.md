# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- **Brand identity.** A new mark — a six-petal aperture flower (pink, peach,
  gold, mint, blue, violet) on a deep-violet tile with an iris core and a white
  sparkle — paired with the full product name on a **single line**: `OBS` in
  Manrope Bold at a muted tone, `Beauty Filter` in Manrope ExtraBold. Ships an
  app icon, horizontal/stacked lockups (dark, light, mono) and a hero banner.
  Sources live in `assets/brand/svg`, exports in `assets/brand/png`, and
  `tools/brand/gen.py` regenerates everything. See
  [`assets/brand/README.md`](assets/brand/README.md) for the palette,
  clearspace and usage rules.
- **Installer branding.** The Windows setup now uses the app icon
  (`SetupIconFile`) and ships `obs-beauty-filter.ico` so the entry in **Apps &
  Features** and the uninstaller show the real logo.

## [1.5.0] - 2026-11-11

### Added

- **Windows `.exe` installer.** `obs-beauty-filter-1.5.0-windows-x64-setup.exe`
  detects the OBS Studio installation, closes a running OBS when needed, and
  installs the plugin into the OBS 33+ third-party plugin directory
  (`%ProgramData%\obs-studio\plugins\obs-beauty-filter`). It creates an entry in
  **Apps & Features** with a working uninstaller and offers to launch OBS when
  setup finishes.
- **Portable OBS support in the installer.** When a portable tree is detected
  (`portable_mode`, `portable_mode.txt`, `obs_portable_mode` or
  `obs_portable_mode.txt`), the plugin is installed into
  `<OBS>\plugins\obs-beauty-filter` instead.
- **Installer smoke test in Windows CI.** The workflow installs the generated
  setup silently into a temporary directory, verifies the DLL/effect/locales
  and uninstaller, then removes everything again.

### Changed

- The installer refuses to run on OBS 32 and older with a message that points
  to the ZIP package and the legacy layout instructions, so it can never
  install the new layout where an older OBS will not find it.
- Windows CI now publishes both the ZIP and the `.exe` in the
  `obs-beauty-filter-windows-x64` artifact.
- README installation instructions now lead with the `.exe` installer and keep
  the ZIP/manual layout as the fallback for older OBS and advanced setups.

## [1.4.0] - 2026-11-06

### Added

- **Custom presets.** Save the current creative settings under a name, reload
  them from the preset dropdown, overwrite an existing name, and delete them.
  Presets are stored in `custom-presets.json` inside the OBS module config
  directory, so they survive scene-collection changes and OBS restarts.
- **Reset button.** Restore every filter control to the built-in Default
  preset without removing and re-adding the filter.
- **GPU failure UI.** If the effect or a render target cannot be created, the
  properties page now shows an error message and the filter passes the source
  through unchanged instead of rendering black.
- **Render matrix test** (`beauty-render-matrix-test`): 8 presets at 360p,
  720p, 1080p and 1440p (opt-in 4K), opaque-alpha checks, 16F finite/range
  checks, and an optional sampled golden image at
  `tests/golden/render-matrix.txt`.
- **Sanitizer build.** `-DBEAUTY_SANITIZE=address,undefined` (or
  `undefined`) builds and tests with ASan/UBSan; Linux CI runs the complete
  suite under sanitizers.
- **Compatibility matrix** and **benchmark guide** for Intel i5-1235U at
  1080p/1440p/4K.
- GitHub issue templates and this changelog.

### Changed

- The property page now has a preset-name field plus Save/Delete/Reset buttons
  directly under the preset dropdown.
- The preset dropdown lists user presets as `Custom: <name>`.
- Manual control changes still switch the selection back to `Custom`, but the
  name field keeps the last saved name so it is easy to overwrite.
- `BEAUTY_SANITIZE` is now a string (`OFF`, `address,undefined` or
  `undefined`) and fails at configure time with a clear message when the
  compiler runtime is unavailable.
- Linux CI uses Clang + compiler-rt for the sanitizer job because Alpine's GCC
  does not ship libasan/libubsan.
- The load test also verifies that the preset/reset/error property controls
  exist.

### Fixed

- Custom preset names are validated case-insensitively against built-in
  presets, so a user preset can never shadow `Natural`, `Cinema`, etc.
- A missing custom preset file, an unreadable file or a hand-edited preset with
  missing keys falls back to the built-in Default preset.
- Saving a preset no longer duplicates an existing name that differs only in
  case.

## [1.3.0] - 2026-10-29

### Added

- Eight built-in starting presets: Default, Natural, Cinema, Studio, Soft,
  Beard, Dark skin and HDR.
- Preset unit test covering table integrity, lookup, apply and defaults.
- Documentation for every preset in English and Vietnamese.

### Changed

- Changing a creative control switches the preset selection to `Custom`.
- README, TUNING and TESTING documents updated for the preset workflow.

## [1.2.0] - 2026-10-20

### Added

- Frequency separation: a reduced-resolution bilateral base plus full-resolution
  detail reconstruction.
- HDR-safe analysis and output for `GS_CS_709_EXTENDED`, `GS_CS_709_SCRGB` and
  `GS_CS_SRGB_16F`.
- Anti-banding triangular dithering for 8-bit output.
- Fast / Balanced / High quality levels.
- D3D11 effect-compilation coverage in Windows CI.

### Fixed

- D3D11 shader compilation failure caused by scalar vector constructors.
- HDR double-scaling when the source was scRGB.
- Windows effect include paths now use forward slashes.

## [1.1.0] - 2026-10-06

### Added

- Initial public release: edge-preserving skin smoothing, layered skin mask,
  heuristic face region, teeth/eye zones, complexion controls, previews,
  opacity and alpha support.
- OBS 33+ plugin layout on Windows, legacy layout for OBS 32 and older.
- Linux and Windows CI builds with packaged artifacts.

[1.5.0]: https://github.com/minhvuong097/obs-beauty-filter/releases/tag/v1.5.0
[1.4.0]: https://github.com/minhvuong097/obs-beauty-filter/releases/tag/v1.4.0
[1.3.0]: https://github.com/minhvuong097/obs-beauty-filter/releases/tag/v1.3.0
[1.2.0]: https://github.com/minhvuong097/obs-beauty-filter/releases/tag/v1.2.0
[1.1.0]: https://github.com/minhvuong097/obs-beauty-filter/releases/tag/v1.1.0
