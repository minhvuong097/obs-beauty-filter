## Summary

<!-- What changed, and why? One or two sentences is enough. -->

## Type of change

- [ ] Bug fix
- [ ] New filter control or preset
- [ ] Shader / visual change
- [ ] Build, CI or packaging
- [ ] Documentation only

## Testing

<!-- Paste the commands you ran and their result. Delete what does not apply. -->

- [ ] `./build/beauty-face-region-test`
- [ ] `./build/beauty-preset-test`
- [ ] `xvfb-run -a ./build/beauty-load-test ...`
- [ ] `BEAUTY_MATRIX_GOLDEN=tests/golden/render-matrix.txt xvfb-run -a ./build/beauty-render-matrix-test ...`
- [ ] Built with `-DBEAUTY_WERROR=ON`
- [ ] Ran `clang-format --dry-run --Werror src/*.c src/*.h tests/*.c`

## Visual evidence

<!-- Before/after screenshots, mask/zone previews or a short clip for shader changes. -->

## Checklist

- [ ] New settings keys were added to `src/beauty-settings.h` and the README
      parameter table in both languages.
- [ ] New locale strings exist in `data/locale/en-US.ini` and
      `data/locale/vi-VN.ini`.
- [ ] Custom presets still round-trip every control, including the new ones.
- [ ] No change to existing preset defaults unless the golden images were
      regenerated on purpose.
