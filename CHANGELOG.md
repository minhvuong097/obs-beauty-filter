# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and this project adheres to [Semantic Versioning](https://semver.org/).

## [1.7.0] - 2026-11-13

### Added

- **Demo images and GIF made with the plugin itself.** The README now shows a
  before/after still, a 100% crop at 200% zoom, a ping-pong GIF of the
  **Amount** slider and a real OBS properties screenshot. Every pixel comes from
  `tools/demo/render-demo.c`, a small offline renderer that starts a headless
  libobs graphics context, loads the built plugin and an `image_source`, and
  reads the processed frames back; ImageMagick and gifski only add captions.
  Build it with `-DBEAUTY_BUILD_DEMO_TOOLS=ON` and regenerate the set with
  `tools/demo/build-assets.sh`. The CC0 source photo, its credit and the
  regeneration steps live in [`assets/demo/README.md`](assets/demo/README.md).
- **Hover tooltips on every control.** OBS uses a property's description as the
  label, so the 1.6.0 layout shortened labels to keep the column aligned. The
  guidance is back as `long_description` (29 keys in English and Vietnamese),
  which OBS renders as a tooltip / help icon. Missing locale keys are skipped
  instead of displaying the raw key.
- **Dark-frame regression test.** `beauty-load-test` now also renders a dark
  skin tone through the light balance and requires the luma to rise, alongside
  the existing bright-frame check.
- **`BEAUTY_DEBUG_LIGHT=1`** prints the measured frame mean, skin mean, skin
  coverage, target and gain on the first analysis tick. It is off by default and
  only exists for field diagnostics.

### Fixed

- **Automatic light balance measured a luma of zero**, so the gain was pinned to
  the 2.5x clamp and bright frames were washed out. The analysis pass sampled
  the source through a uniform that reads back as 0 on the Mesa OpenGL driver,
  and it also only ran when skin detection or face tracking was on. Luma is now
  written into the **b** channel by the mask pass (where the sampled source is
  already bound), the analysis pass only averages it, and the mask pass runs
  whenever light balance is enabled. Verified on a real frame: gain converges to
  0.97 instead of 2.5, average luma 0.573 -> 0.571, a bright test frame goes
  161 -> 117 and a dark one 69 -> 105.
- **Wrong exposure target for portraits and dark scenes.** The measurement now
  uses two branches: when skin covers 2-90% of the frame the face sets the
  exposure (`0.85 * skin_mean + 0.15 * frame_mean`, target 0.30), otherwise the
  whole frame is used (target 0.18). A bright window behind the subject no
  longer darkens the face, and scenes without a clear subject are still lifted.
- **Slow recovery after a lighting change.** The gain ramp keeps its 0.25 rate
  for small corrections but adds 0.45 when the error exceeds 0.75, so walking
  into a dark room settles in a few frames instead of several seconds.

## [1.6.0] - 2026-11-12

### Added

- **Automatic light balance.** A new **Auto light balance** control in
  **Detail & light** measures scene brightness from the existing 64 px analysis
  readback (skin-weighted when a face is present) and lifts or lowers exposure
  toward a neutral level. A soft highlight shoulder protects SDR highlights,
  HDR sources are never clipped, and the measured gain is smoothed over several
  frames so lighting changes fade instead of pumping. `0` turns it off, `0.35`
  is the default and `1` is the strongest correction (gain capped at 2.5x).
- **New property page layout.** Controls are grouped as *Skin smoothing*,
  *Complexion*, *Detail & light*, *Face & eyes* and *Performance & preview*,
  with short labels that stay aligned instead of long sentences.
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

### Changed

- Short, consistent property labels in English and Vietnamese. OBS uses a
  property's description as the slider label, so the previous long help text
  was truncated and pushed controls out of alignment; labels now fit on one
  line and the controls line up in a single column.
- Preset actions are labelled **Save preset**, **Delete preset** and
  **Reset to defaults**.

### Fixed

- Changing a slider could appear to do nothing because the property page was
  rebuilt on every modification callback. Manual edits now only mark the
  preset as *Custom* and leave the page (and the slider handle) alone.

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

[1.7.0]: https://github.com/minhvuong097/obs-beauty-filter/releases/tag/v1.7.0
[1.6.0]: https://github.com/minhvuong097/obs-beauty-filter/releases/tag/v1.6.0
[1.5.0]: https://github.com/minhvuong097/obs-beauty-filter/releases/tag/v1.5.0
[1.4.0]: https://github.com/minhvuong097/obs-beauty-filter/releases/tag/v1.4.0
[1.3.0]: https://github.com/minhvuong097/obs-beauty-filter/releases/tag/v1.3.0
[1.2.0]: https://github.com/minhvuong097/obs-beauty-filter/releases/tag/v1.2.0
[1.1.0]: https://github.com/minhvuong097/obs-beauty-filter/releases/tag/v1.1.0
