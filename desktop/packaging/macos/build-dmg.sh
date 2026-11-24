#!/usr/bin/env bash
# Deploys Qt into AnyPC.app, ad-hoc signs it and creates AnyPC-<version>-macOS.dmg.
# Usage: packaging/macos/build-dmg.sh <build-dir> <version> <output-dir>
set -euo pipefail

BUILD_DIR=$(cd "$1" && pwd)
VERSION=$2
mkdir -p "$3"
OUTPUT_DIR=$(cd "$3" && pwd)
APP="$BUILD_DIR/AnyPC.app"
STAGING="$BUILD_DIR/dmg-staging"

macdeployqt "$APP" -always-overwrite -verbose=1

# Ad-hoc signature (required on Apple Silicon). Replace "-" with a Developer ID
# identity and add notarisation for distribution without Gatekeeper warnings.
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"

rm -rf "$STAGING"
mkdir -p "$STAGING"
cp -R "$APP" "$STAGING/"
ln -s /Applications "$STAGING/Applications"

DMG="$OUTPUT_DIR/AnyPC-$VERSION-macOS.dmg"
rm -f "$DMG"
hdiutil create -volname "AnyPC $VERSION" -srcfolder "$STAGING" -ov -format UDZO "$DMG"
echo "Created $DMG"
