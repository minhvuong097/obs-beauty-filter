# Contributing to OBS Beauty Filter

Thanks for helping. This project is maintained by volunteers, ships no paid
services, and every dependency and CI runner it uses is free for open source.

## Ground rules

- Be civil; assume good faith.
- One logical change per pull request. Separate refactors from behaviour changes.
- Keep the plugin dependency-free: no new runtime library, no network access, no
  telemetry, no AI model unless it can be bundled and licensed for free.
- Do not add anything that requires a paid account or certificate (code-signing
  subscription, Apple Developer Program, paid cloud inference). See
  [SECURITY.md](SECURITY.md#verifying-a-download) for how releases are verified
  instead.

## Building

```bash
# Linux
./scripts/build-linux.sh                 # Release into ./build, prefix /usr
BUILD_TYPE=RelWithDebInfo ./scripts/build-linux.sh

# Windows (PowerShell)
.\scripts\build-windows.ps1
```

Manual CMake is fine too:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DBEAUTY_BUILD_TESTS=ON -DBEAUTY_WERROR=ON \
      -DBEAUTY_BUILD_DEMO_TOOLS=ON
cmake --build build -j"$(nproc)"
```

Or use the checked-in presets: `cmake --preset dev` then
`cmake --build --preset dev`. The `release` and `sanitize` presets mirror CI.

You need OBS headers plus `libobsConfig.cmake` from a build or install tree. On
Alpine the CI recipe in `.github/scripts/build-linux-ci.sh` installs everything.

## Tests

Run these before opening a pull request:

```bash
./build/beauty-face-region-test
./build/beauty-preset-test
xvfb-run -a ./build/beauty-render-matrix-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter
xvfb-run -a ./build/beauty-load-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter \
  /usr/lib/obs-plugins/image-source.so \
  /usr/share/obs/obs-plugins/image-source
```

`BEAUTY_MATRIX_GOLDEN=tests/golden/render-matrix.txt` enables the Linux golden
comparison (leave it unset on other GPUs/platforms). Expected output and the
manual checklist are in [docs/TESTING.md](docs/TESTING.md).

Before pushing, also run the two gates that Linux CI enforces:

```bash
cmake --build build --target format-check
# or: clang-format --dry-run --Werror src/*.c src/*.h tests/*.c
```

On a visual change, regenerate and review `tests/golden/render-matrix.txt`;
never edit it by hand. The new controls default to `0`, so adding one should
not change the golden file.

If your change touches the shader, rendering or the property page, also confirm
it in a real OBS window; CI only proves that the effect compiles and that the
invariants hold.

## Code style

- C99, 8-column tabs, wrapped at 100 columns, K&R braces. `.clang-format` is
  authoritative: run `cmake --build build --target format` (or
  `clang-format -i src/*.c src/*.h tests/*.c`) instead of hand-aligning. Linux
  CI fails when `format-check` would change a file. Table-like initialisers can
  be fenced with `// clang-format off/on`.
- Build with `-DBEAUTY_WERROR=ON`; the extra warnings (`-Wshadow`,
  `-Wstrict-prototypes`, `-Wmissing-prototypes`, `-Wpointer-arith`,
  `-Wcast-qual`) are part of the contract.
- No C++ in the plugin. The offline demo tooling may stay simple C as well.
- Keep GPU work on the render thread; never read a full-resolution frame back.
- Readbacks must stay asynchronous: stage on one frame, map on the next (see
  `beauty_consume_analysis()`), never map a copy queued in the same frame.
- Comments explain *why* a decision was made, not what the line does.

## UI strings and tooltips

- Labels are short because OBS renders a property's description as the slider
  label. Put the sentence in `long_description` through `add_tooltips()`.
- Add every key to **both** `data/locale/en-US.ini` and
  `data/locale/vi-VN.ini`; a missing key is skipped at runtime, which is harder
  to notice than a compile error.
- Vietnamese and English documentation are kept in parallel files
  (`README.md`/`README.en.md`, `docs/*.md`/`docs/*.en.md`). Update both.
- A new control needs a key in `src/beauty-settings.h`, a property in
  `beauty_properties()`, a default in `beauty_defaults()`, a uniform in
  `beauty_check_effect_params()`, both locale files and the README parameter
  table. The load test asserts that every key exists in the property page.
- If a new key must be saved in custom presets, bump
  `BEAUTY_CUSTOM_SCHEMA_VERSION` and keep the old schema readable (missing keys
  fall back to the Default preset).

## Versioning and releases

- Semantic versioning; each release bumps the **minor** part: 1.9.0 → 1.10.0.
- `BEAUTY_FILTER_VERSION` in `CMakeLists.txt` is the single source of truth; the
  demo captions read it automatically. Also bump `installer/obs-beauty-filter.iss`,
  the README download names and `CHANGELOG.md` in the same commit.
- Pushing a `v*` tag runs the full build and test matrix; the `release` job
  then publishes every artifact plus `SHA256SUMS.txt` through `gh release
  create`. Do not upload binaries by hand. The body starts from GitHub's
  generated notes: edit it after publishing so it matches the `CHANGELOG.md`
  entry in **both languages, Vietnamese first then English** (the README
  default is Vietnamese).

## Assets

- Code is GPL-2.0-or-later. Brand and demo images are **not** GPL; their terms
  are documented in `assets/brand/README.md` and `assets/demo/README.md`.
- Demo pictures must be real plugin output, not mock-ups. Regenerate them with
  `tools/demo/build-assets.sh` after a visual change, and keep the CC0 credit.
- Never commit an image whose licence does not allow redistribution.

## Commits and pull requests

- Subject line in the imperative mood, ≤ 72 characters: `Fix mask pass when...`.
- Explain the user-visible effect in the body and reference the issue.
- Sign-off (`git commit -s`) is welcome but not required; there is no CLA.
- A pull request is ready when the four tests above pass locally and, for visual
  changes, screenshots or numbers are attached.
