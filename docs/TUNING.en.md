# Tuning Guide

**English | [Tiếng Việt](TUNING.md)**

## 1. Recommended setup order

1. **Lighting and white balance first.** A beauty filter cannot fix purple/green lighting.
2. **Denoise before Beauty** if the webcam is noisy. Smoothing noisy footage creates smears.
3. **Skin mask**: enable **Show skin mask** and adjust **Skin sensitivity** until skin (face, neck, hands) is almost fully white while eyes/lips/hair/background stay black.
4. **Face-aware zones**: keep it on, then enable **Show face zones** for a few seconds to confirm the face box, eyes and mouth are aligned. If not, use **Manual face box**.
5. **Amount + Radius**: raise until it looks right, then back off 10–20%.
6. **Detail preservation**: raise if skin looks flat; lower if blemishes/texture remain.
7. **Clarity**: add a little to keep eyes and hair sharp.
8. **Complexion**: adjust brightness/warmth/rosy in small amounts.
9. **Temporal mask stability**: raise if the mask flickers; lower if fast head turns produce ghosting.
10. **Teeth whitening / Eye brightening**: enable last, at small values, and verify with **Show face zones**.
11. **Split preview** to check, then turn it off before going live.

## 2. Suggested presets

### Natural — 1080p webcam, indoor lighting

| Parameter | Value |
|---|---:|
| Amount | 0.35 |
| Radius | 6 |
| Detail | 0.40 |
| Sensitivity | 0.50 |
| Brightness | 0 |
| Warmth | 3 |
| Saturation | 0 |
| Rosy | 0.12 |
| Clarity | 0.10 |
| Glow | 0.05 |
| Face-aware zones | On |
| Temporal stability | 0.60 |
| Teeth / Eyes | 0 / 0 |
| Quality | Balanced |

### Studio — close-up, discrete GPU

| Parameter | Value |
|---|---:|
| Amount | 0.60 |
| Radius | 10 |
| Detail | 0.28 |
| Sensitivity | 0.50 |
| Brightness | 4 |
| Warmth | 2 |
| Saturation | 2 |
| Rosy | 0.18 |
| Clarity | 0.12 |
| Glow | 0.10 |
| Face-aware zones | On |
| Temporal stability | 0.55 |
| Teeth / Eyes | 0.35 / 0.20 |
| Quality | High |

### Soft — dry skin, soft lighting

| Parameter | Value |
|---|---:|
| Amount | 0.45 |
| Radius | 8 |
| Detail | 0.30 |
| Sensitivity | 0.55 |
| Brightness | 3 |
| Warmth | 4 |
| Saturation | 4 |
| Rosy | 0.20 |
| Clarity | 0.08 |
| Glow | 0.18 |
| Face-aware zones | On |
| Temporal stability | 0.65 |
| Teeth / Eyes | 0.25 / 0.15 |
| Quality | Balanced |

### Men / beards

| Parameter | Value |
|---|---:|
| Amount | 0.22 |
| Radius | 5 |
| Detail | 0.60 |
| Sensitivity | 0.45 |
| Brightness | 0 |
| Warmth | 0 |
| Saturation | 0 |
| Rosy | 0.05 |
| Clarity | 0.12 |
| Glow | 0 |
| Face-aware zones | On |
| Temporal stability | 0.50 |
| Teeth / Eyes | 0.20 / 0.10 |
| Quality | Balanced |

### Deep skin tones

| Parameter | Value |
|---|---:|
| Amount | 0.30 |
| Radius | 6 |
| Detail | 0.45 |
| Sensitivity | 0.65–0.75 |
| Brightness | 3–6 |
| Warmth | 0–3 |
| Saturation | 2–5 |
| Rosy | 0.05–0.10 |
| Clarity | 0.10 |
| Glow | 0.05 |
| Face-aware zones | On |
| Temporal stability | 0.60 |
| Teeth / Eyes | 0.30 / 0.15 |
| Quality | Balanced |

Always check **Show skin mask**: if dark skin is not white, raise Sensitivity; if hair/background also turns white, lower it.

### HDR — Rec.709 extended / scRGB

| Parameter | Value |
|---|---:|
| Amount | 0.25–0.40 |
| Radius | 4–7 |
| Detail | 0.40–0.55 |
| Glow | 0–0.05 |
| Face-aware zones | On |
| Temporal stability | 0.50–0.65 |
| Quality | Fast / Balanced |

HDR has a wider brightness range, so Glow and Rosy can easily look excessive. Start lower than the SDR presets and check highlights (lights, windows) for clipping.

## 3. Tuning the face zones

### When to enable them

- Enable **Face-aware zones** when using **Teeth whitening** or **Eye brightening**.
- The face prior also reduces false positives on skin-coloured walls and furniture.
- Disable it when you do not need teeth/eye processing and want to save GPU time.

### Checking the face box

1. Enable **Show face zones**.
2. Expected colours:
   - **green**: face prior (face + neck),
   - **blue**: the two eye ellipses,
   - **yellow**: the mouth ellipse.
3. If the box is wrong:
   - Enable **Manual face box**.
   - Adjust **Face center X/Y** to centre it on the face.
   - Adjust **Face width/height** to fit. These are **full dimensions**, not half-extents.
4. Disable **Show face zones** when finished.

### Recommended amounts

- **Teeth whitening**: `0.2–0.5`. Above `0.6`, lips/gums can look washed out.
- **Eye brightening**: `0.1–0.3`. Above `0.4`, eyes can look unnaturally white or glow around the socket.
- **Temporal stability**: `0.4–0.7` is the safe range. Above `0.8` fast head turns can ghost; below `0.3` the mask may flicker.
- If skin around the eyes/mouth also brightens, lower Teeth/Eyes first, then shrink the face box.

## 4. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Skin looks flat, texture lost | Amount/Radius too high, Detail too low | Lower Amount/Radius, raise Detail |
| Eyes/hair look soft | Low Clarity, high Radius | Raise Clarity, lower Radius |
| Skin-coloured wood/wall gets smoothed | Mask picks up skin colours | Show skin mask; lower Sensitivity; enable Face-aware zones; use Manual face box if needed |
| Lips look pale/blurred | Mask covers lips | Lower Sensitivity; lower Amount; add Clarity |
| Skin too red/pink | High Rosy/Warmth | Lower Rosy and Warmth |
| Skin looks yellow/green | Wrong source white balance | Fix white balance first; use a little Warmth |
| Mask flickers during motion | Low temporal value, per-pixel mask changes | Raise Temporal stability (0.6–0.75); lower Amount; raise Detail |
| Ghosting on fast head turns | Temporal too high | Lower Temporal stability (0.3–0.5) |
| Teeth whitening does nothing | No face box, teeth too dark/yellow, or wrong zone | Enable Face-aware zones; Show face zones; use Manual face box; raise Teeth moderately |
| Teeth whitening also whitens lips | Mouth zone too large or Teeth too high | Shrink Face height/width; lower Teeth |
| Eye brightening brightens skin | Eye zones too large or Eyes too high | Lower Eyes; shrink Face width/height; check zone preview |
| Face box jumps around | Multiple people, skin-coloured background, occluded face | Use Manual face box; lower Sensitivity; disable Face-aware if not using teeth/eye |
| FPS drops | High Quality/Radius, 4K, face-aware on | Lower Quality, lower Radius, disable Glow, disable Face-aware if unused |
| HDR highlights clip | Glow/Rosy too high or source already clipped | Lower Glow/Rosy; check the source; the filter does not tone-map |
| No visible effect | Amount/Opacity = 0, or unsupported source colour space | Check Amount/Opacity; read the OBS log |
| Filter does not appear | Wrong DLL/data location | Check the install layout and OBS log |

## 5. Working with multiple skin tones

- Do not use one preset for everyone.
- Check the mask under three conditions: bright light, low light and backlight.
- Avoid Amount > 0.6 on deep skin tones unless you have verified it; detail can disappear.
- For very light skin, lower Brightness and Rosy to avoid clipping highlights.
- For deep skin tones, raise Sensitivity but check the face box; the face prior can cut off skin outside the ellipse if the box is too small.
- If a stream has multiple people, use a separate filter per camera/source rather than one shared filter. The detector currently handles one face box.

## 6. Recommended filter order

```
Camera → Denoise → Color Correction (if needed) → Beauty Filter → Sharpen (very light) → Scene
```

- Put Beauty **after** denoise so noise is not blurred into patches.
- Put Sharpen **after** Beauty if you need detail back, but only 0.05–0.15.
- Do not stack several smoothing filters (Beauty + Blur + Sharpen); it creates halos.
