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
      -DBEAUTY_BUILD_TESTS=ON -DBEAUTY_BUILD_DEMO_TOOLS=ON
cmake --build build -j"$(nproc)"
```

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

If your change touches the shader, rendering or the property page, also confirm
it in a real OBS window; CI only proves that the effect compiles and that the
invariants hold.

## Code style

- C99, 8-column tabs, wrapped at 100 columns, K&R braces. Follow the surrounding
  code; there is no automatic formatter gate.
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

## Versioning and releases

- Semantic versioning; each release bumps the **minor** part: 1.8.0 → 1.9.0.
- `BEAUTY_FILTER_VERSION` in `CMakeLists.txt` is the single source of truth; the
  demo captions read it automatically. Also bump `installer/obs-beauty-filter.iss`,
  the README download names and `CHANGELOG.md` in the same commit.
- The release notes are taken from the `CHANGELOG.md` entry and published in
  **both languages, Vietnamese first then English** (the README default is
  Vietnamese). The artifacts must come from the CI run on the tag.
  `SHA256SUMS.txt` is generated from the downloaded artifacts and verified again
  after the release is published.

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
