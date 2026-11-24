#!/usr/bin/env bash
# Builds AnyPC-<version>-x86_64.AppImage from a configured CMake build directory.
# Usage: packaging/linux/build-appimage.sh <build-dir> <version> <output-dir>
set -euo pipefail

BUILD_DIR=$(realpath "$1")
VERSION=$2
OUTPUT_DIR=$(realpath -m "$3")
SOURCE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
WORK_DIR="$BUILD_DIR/appimage"
APPDIR="$WORK_DIR/AppDir"
TOOLS="$WORK_DIR/tools"

rm -rf "$APPDIR"
mkdir -p "$APPDIR" "$TOOLS" "$OUTPUT_DIR"

cmake --install "$BUILD_DIR" --prefix "$APPDIR/usr"

fetch() {
  local name=$1 url=$2
  [[ -x "$TOOLS/$name" ]] || { curl -fsSL -o "$TOOLS/$name" "$url"; chmod +x "$TOOLS/$name"; }
}
fetch linuxdeploy-x86_64.AppImage \
  https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fetch linuxdeploy-plugin-qt-x86_64.AppImage \
  https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage

cp "$SOURCE_DIR/resources/icons/anypc-256.png" "$WORK_DIR/anypc.png"

export PATH="$TOOLS:$PATH"
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE=${QMAKE:-$(command -v qmake6 || command -v qmake)}
export LINUXDEPLOY_OUTPUT_VERSION="$VERSION"
export EXTRA_PLATFORM_PLUGINS="libqxcb.so"

cd "$WORK_DIR"
linuxdeploy-x86_64.AppImage \
  --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/anypc" \
  --desktop-file "$APPDIR/usr/share/applications/anypc.desktop" \
  --icon-file "$WORK_DIR/anypc.png" \
  --plugin qt \
  --output appimage

mv "$WORK_DIR"/AnyPC-*.AppImage "$OUTPUT_DIR/AnyPC-$VERSION-x86_64.AppImage"
echo "Created $OUTPUT_DIR/AnyPC-$VERSION-x86_64.AppImage"
