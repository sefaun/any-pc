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

# An ad-hoc signature's default "designated requirement" is the hash of this
# exact build, so macOS treats every release as a different app: Screen
# Recording and Accessibility permissions granted to the previous version
# silently stop applying (toggling the old entry does not help). Re-sign the
# app itself with a requirement that only names the bundle identifier so the
# permissions carry over to future updates.
BUNDLE_ID=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$APP/Contents/Info.plist")
codesign --force --sign - -r="designated => identifier \"$BUNDLE_ID\"" "$APP"
codesign --verify --deep --strict "$APP"
codesign --display --requirements - "$APP"

rm -rf "$STAGING"
mkdir -p "$STAGING"
cp -R "$APP" "$STAGING/"
ln -s /Applications "$STAGING/Applications"

DMG="$OUTPUT_DIR/AnyPC-$VERSION-macOS.dmg"
rm -f "$DMG"
hdiutil create -volname "AnyPC $VERSION" -srcfolder "$STAGING" -ov -format UDZO "$DMG"
echo "Created $DMG"
