#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr}"
INSTALL_LIBDIR="${INSTALL_LIBDIR:-}"

CMAKE_ARGS=(
  -S "${ROOT_DIR}"
  -B "${BUILD_DIR}"
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"
)

if [[ -n "${INSTALL_LIBDIR}" ]]; then
  CMAKE_ARGS+=(-DCMAKE_INSTALL_LIBDIR="${INSTALL_LIBDIR}")
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build "${BUILD_DIR}" -j"$(nproc)"

cat <<EOF

Build xong: ${BUILD_DIR}
Cài đặt:    sudo cmake --install ${BUILD_DIR}
Chạy test:  cmake -S . -B build -DBEAUTY_BUILD_TESTS=ON && cmake --build build

Debian/Ubuntu multiarch:
  INSTALL_LIBDIR="lib/\$(dpkg-architecture -qDEB_HOST_MULTIARCH)" ./scripts/build-linux.sh
EOF
