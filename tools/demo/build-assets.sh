#!/bin/sh
#
# Regenerate the README demo images from the real plugin.
#
# The before/after stills and the Amount GIF are produced by rendering a photo
# through the actual filter on a real libobs graphics context
# (tools/demo/render-demo.c), then compositing the frames with ImageMagick and
# ffmpeg.  Nothing here is a mock-up.
#
# Prerequisites:
#   - a standalone plugin build with -DBEAUTY_BUILD_DEMO_TOOLS=ON
#   - ImageMagick 7 (magick) and gifski (or ffmpeg as a fallback)
#   - Xvfb when running on a headless machine (the script adds xvfb-run)
#
# Usage:
#   tools/demo/build-assets.sh [photo]
#
# Environment overrides:
#   BEAUTY_PLUGIN        path to obs-beauty-filter.so/.dll
#   BEAUTY_PLUGIN_DATA   path to the plugin data directory
#   IMAGE_SOURCE_PLUGIN  path to image-source.so/.dll
#   IMAGE_SOURCE_DATA    path to the image-source data directory
#   BUILD_DIR            build directory (default: build)
#   OBS_PANEL            optional OBS properties screenshot to copy in
#
# Copyright (C) 2026 OBS Beauty Filter contributors
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
build_dir=${BUILD_DIR:-$root/build}
out_dir=$root/assets/demo

photo=${1:-$out_dir/source-photo.jpg}
plugin=${BEAUTY_PLUGIN:-$build_dir/obs-beauty-filter.so}
plugin_data=${BEAUTY_PLUGIN_DATA:-$build_dir/rundir/share/obs/obs-plugins/obs-beauty-filter}
image_plugin=${IMAGE_SOURCE_PLUGIN:-/usr/lib/obs-plugins/image-source.so}
image_data=${IMAGE_SOURCE_DATA:-/usr/share/obs/obs-plugins/image-source}
renderer=$build_dir/beauty-demo-render
version=$(sed -n 's/.*project(obs-beauty-filter VERSION \([0-9.]*\) LANGUAGES C).*/\1/p' \
	"$root/CMakeLists.txt")
version=${version:-dev}

bold='DejaVu-Sans-Bold'
regular='DejaVu-Sans'
bar='#14161c'
ink='#e8ebf2'
muted='#9aa4b8'

for f in "$photo" "$plugin" "$image_plugin" "$renderer"; do
	if [ ! -e "$f" ]; then
		echo "missing: $f" >&2
		exit 1
	fi
done

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

run() {
	if [ -n "${DISPLAY:-}" ]; then
		"$@"
	else
		xvfb-run -a "$@"
	fi
}

# --------------------------------------------------------------------------
# 1. Render the frames through the plugin
# --------------------------------------------------------------------------

echo "rendering frames through the plugin..."
run "$renderer" "$plugin" "$plugin_data" "$image_plugin" "$image_data" \
	"$photo" "$work" \
	--width 1024 --height 683 \
	--amount 0.62 --radius 9 --detail 0.4 --light 0.45 \
	--rosy 0.12 --clarity 0.08 --settle 24 \
	--sweep 0,0.15,0.3,0.45,0.6,0.75,0.9 >/dev/null

for f in before after sweep-000 sweep-001 sweep-002 sweep-003 sweep-004 \
	sweep-005 sweep-006; do
	magick "$work/$f.ppm" -depth 8 "$work/$f.png"
done

# --------------------------------------------------------------------------
# 2. Before/after still
# --------------------------------------------------------------------------

caption() {
	# caption <image> <text> <marker-colour> <out>
	magick -size 860x42 "xc:$bar" \
		-fill "$ink" -font "$bold" -pointsize 17 -gravity west \
		-annotate +18+1 "$3" \
		-fill "$muted" -font "$regular" -pointsize 16 \
		-annotate +106+1 "$2" "$4"
}

magick "$work/before.png" -resize 860x574^ -gravity center -extent 860x574 \
	"$work/before-s.png"
magick "$work/after.png" -resize 860x574^ -gravity center -extent 860x574 \
	"$work/after-s.png"

caption "$work/before-s.png" "original camera" "BEFORE" "$work/cap-b.png"
caption "$work/after-s.png" "Beauty Filter $version" "AFTER" "$work/cap-a.png"

magick "$work/cap-b.png" "$work/before-s.png" -append "$work/panel-b.png"
magick "$work/cap-a.png" "$work/after-s.png" -append "$work/panel-a.png"
# JPEG keeps the hero image below ~350 kB without smoothing away the skin
# texture the comparison is meant to show, hence 4:4:4 chroma and q92.
magick "$work/panel-b.png" -size 2x616 "xc:$bar" "$work/panel-a.png" \
	+append -depth 8 -strip -sampling-factor 4:4:4 -quality 92 \
	"$out_dir/before-after.jpg"

# --------------------------------------------------------------------------
# 3. Skin detail close-up (200% crop, same region on both sides)
# --------------------------------------------------------------------------

magick "$work/before.png" -crop 300x240+470+140 +repage -resize 200% \
	"$work/crop-b.png"
magick "$work/after.png" -crop 300x240+470+140 +repage -resize 200% \
	"$work/crop-a.png"

magick -size 600x38 "xc:$bar" -fill "$ink" -font "$bold" -pointsize 15 \
	-gravity west -annotate +14+1 "BEFORE" -fill "$muted" -font "$regular" \
	-pointsize 14 -annotate +96+1 "100% crop, 200% zoom" "$work/ccap-b.png"
magick -size 600x38 "xc:$bar" -fill "$ink" -font "$bold" -pointsize 15 \
	-gravity west -annotate +14+1 "AFTER" -fill "$muted" -font "$regular" \
	-pointsize 14 -annotate +96+1 "skin + detail preserved" "$work/ccap-a.png"

magick "$work/ccap-b.png" "$work/crop-b.png" -append "$work/cpanel-b.png"
magick "$work/ccap-a.png" "$work/crop-a.png" -append "$work/cpanel-a.png"
magick "$work/cpanel-b.png" -size 2x518 "xc:$bar" "$work/cpanel-a.png" \
	+append -depth 8 -strip -define png:compression-level=9 \
	"$out_dir/skin-detail.png"

# --------------------------------------------------------------------------
# 4. Amount sweep GIF (ping-pong)
# --------------------------------------------------------------------------

echo "building the Amount GIF..."
values='0.00 0.15 0.30 0.45 0.60 0.75 0.90'
i=0
for value in $values; do
	file=$(printf 'sweep-%03d' "$i")
	magick "$work/$file.png" -resize 640x427 \
		-background "$bar" -gravity south -splice 0x30 \
		-fill "$ink" -font "$bold" -pointsize 14 -gravity southwest \
		-annotate +12+7 "Amount $value" \
		-fill "$muted" -font "$regular" -pointsize 11 -gravity southeast \
		-annotate +12+8 "obs-beauty-filter" "$work/frame-$i.png"
	i=$((i + 1))
done

# Forward then backward, so the loop never jumps.
mkdir -p "$work/seq"
n=0
for i in 0 1 2 3 4 5 6 5 4 3 2 1 0; do
	n=$((n + 1))
	cp "$work/frame-$i.png" "$work/seq/$(printf '%03d' "$n").png"
done

# gifski keeps far more skin detail than a global 256-colour palette and is
# roughly half the size; fall back to ffmpeg on machines without it.
if command -v gifski >/dev/null 2>&1; then
	gifski --fps 8 --width 640 --quality 90 \
		-o "$out_dir/amount-slider.gif" "$work"/seq/*.png
elif command -v ffmpeg >/dev/null 2>&1; then
	ffmpeg -y -loglevel error -framerate 8 -i "$work/seq/%03d.png" \
		-vf "split[a][b];[a]palettegen=max_colors=256:stats_mode=full[p];[b][p]paletteuse=dither=sierra2_4a" \
		-loop 0 "$out_dir/amount-slider.gif"
else
	echo "gifski or ffmpeg is required for the GIF" >&2
	exit 1
fi

# --------------------------------------------------------------------------
# 5. OBS properties panel (real screenshot, optional)
# --------------------------------------------------------------------------

if [ -n "${OBS_PANEL:-}" ]; then
	magick "$OBS_PANEL" -depth 8 -strip "$out_dir/obs-panel.png"
fi

echo
echo "demo assets written to $out_dir:"
ls -lh "$out_dir"/before-after.jpg "$out_dir"/skin-detail.png \
	"$out_dir"/amount-slider.gif