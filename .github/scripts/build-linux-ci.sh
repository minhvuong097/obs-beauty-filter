#!/bin/sh
set -eu

apk add --no-cache \
  build-base \
  cmake \
  mesa-dri-gallium \
  obs-studio \
  obs-studio-dev \
  pkgconf \
  xvfb \
  xvfb-run

rm -rf build package
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

cmake --install build --prefix package
tar -C package -czf obs-beauty-filter-linux-x86_64.tar.gz .
