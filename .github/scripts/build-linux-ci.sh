#!/bin/sh
set -eu

apk add --no-cache \
  build-base \
  clang \
  cmake \
  compiler-rt \
  mesa-dri-gallium \
  obs-studio \
  obs-studio-dev \
  pkgconf \
  xvfb \
  xvfb-run

# Alpine's GCC does not ship libasan/libubsan, so the sanitizer build uses
# Clang plus compiler-rt.  The release build stays on GCC.

rm -rf build build-sanitize package
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBEAUTY_BUILD_TESTS=ON

cmake --build build -j"$(nproc)"

./build/beauty-face-region-test
./build/beauty-preset-test
xvfb-run -a ./build/beauty-load-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter \
  /usr/lib/obs-plugins/image-source.so \
  /usr/share/obs/obs-plugins/image-source
BEAUTY_MATRIX_GOLDEN=tests/golden/render-matrix.txt \
xvfb-run -a ./build/beauty-render-matrix-test \
  ./build/obs-beauty-filter.so \
  ./build/rundir/share/obs/obs-plugins/obs-beauty-filter

# Memory/UB check.  Leak detection is disabled because libobs and Mesa keep
# process-lifetime allocations that are not owned by this plugin.  The
# sanitizer build is separate so the packaged binary stays optimised.
CC=clang cmake -S . -B build-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBEAUTY_BUILD_TESTS=ON \
  -DBEAUTY_SANITIZE=address,undefined
cmake --build build-sanitize -j"$(nproc)"

ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
./build-sanitize/beauty-face-region-test
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
./build-sanitize/beauty-preset-test
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
xvfb-run -a ./build-sanitize/beauty-load-test \
  ./build-sanitize/obs-beauty-filter.so \
  ./build-sanitize/rundir/share/obs/obs-plugins/obs-beauty-filter \
  /usr/lib/obs-plugins/image-source.so \
  /usr/share/obs/obs-plugins/image-source
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
BEAUTY_MATRIX_GOLDEN=tests/golden/render-matrix.txt \
xvfb-run -a ./build-sanitize/beauty-render-matrix-test \
  ./build-sanitize/obs-beauty-filter.so \
  ./build-sanitize/rundir/share/obs/obs-plugins/obs-beauty-filter

cmake --install build --prefix package
tar -C package -czf obs-beauty-filter-linux-x86_64.tar.gz .
