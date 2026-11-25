#!/usr/bin/env bash
# Builds scene/main.cpp into web/kitchen-engine.{js,wasm} and copies the kit's
# page scripts beside them, against the pinned Strata 0.10.0 wasm release.
# Needs emsdk 6.0.11 (EMSDK, default /root/emsdk). Downloads into .strata/.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(dirname "$here")"
STRATA_VERSION=0.10.0
STRATA_SHA256=bb86b974c7e0309db91625ba5ee157273704c77d93f4338c8a63ee625e3d56de
tarball="$repo/.strata/strata-$STRATA_VERSION-wasm32-emscripten.tar.gz"
if [ ! -f "$tarball" ]; then
  mkdir -p "$repo/.strata"
  gh release download "v$STRATA_VERSION" -R Preponderous-Software/strata \
    -p "strata-$STRATA_VERSION-wasm32-emscripten.tar.gz" -D "$repo/.strata"
fi
echo "$STRATA_SHA256  $tarball" | sha256sum -c --quiet -
[ -f "$repo/.strata/kit/build-kit.sh" ] || tar -xzf "$tarball" -C "$repo/.strata"
if ! command -v emcc >/dev/null 2>&1; then
  _py="$(ls -d "$HOME"/.local/share/uv/python/cpython-3.12*/bin/python3.12 2>/dev/null | head -1 || true)"
  export EMSDK_PYTHON="${EMSDK_PYTHON:-$_py}"; _keep="$EMSDK_PYTHON"
  # shellcheck disable=SC1091
  . "${EMSDK:-/root/emsdk}/emsdk_env.sh" >/dev/null 2>&1
  export EMSDK_PYTHON="$_keep"
fi
export KIT_WORK="$repo/.strata/kit-build"
STRATA_TARBALL="$tarball" STRATA_SHA256="$STRATA_SHA256" \
  "$repo/.strata/kit/build-kit.sh" "$here/kit.conf"
