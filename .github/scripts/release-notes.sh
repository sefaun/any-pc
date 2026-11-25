#!/usr/bin/env bash
# Prints the release notes for AnyPC <version>.
#
#   release-notes.sh <version> <owner> <files-dir> <pending|final>
#
# Files found in <files-dir> are listed by name. Missing files are shown as
# "building…" while the release is still being built ("pending") and as
# "not available" once every build has finished ("final").
set -euo pipefail

VERSION=$1
OWNER=$2
DIR=$3
MODE=$4

row() {
  local label=$1 file=$2
  if [[ -f "$DIR/$file" ]]; then
    echo "| $label | \`$file\` |"
  elif [[ "$MODE" == pending ]]; then
    echo "| $label | _building…_ |"
  else
    echo "| $label | _not available in this release_ |"
  fi
}

echo "## Desktop"
echo
echo "| Platform | File |"
echo "| --- | --- |"
row "Windows 10/11 (x64)" "AnyPC-Setup-${VERSION}-x64.exe"
row "macOS 12+ (Universal)" "AnyPC-${VERSION}-macOS.dmg"
row "Linux x86_64 (X11)" "AnyPC-${VERSION}-x86_64.AppImage"
echo
echo "## Mobile"
echo
echo "| Platform | File |"
echo "| --- | --- |"
row "Android (APK, install directly)" "AnyPC-Mobile-${VERSION}-android.apk"
row "Android (AAB, Google Play)" "AnyPC-Mobile-${VERSION}-android.aab"
row "iOS (unsigned IPA)" "AnyPC-Mobile-${VERSION}-ios-unsigned.ipa"
echo
echo "- **Android:** allow installing from unknown sources and open the APK."
echo "- **iOS:** the IPA is unsigned; sign and install it with your Apple ID (e.g. Sideloadly or AltStore)."
echo
echo "## Backend"
echo
echo '```bash'
echo "docker pull ghcr.io/${OWNER}/any-pc-backend:${VERSION}"
echo '```'
if [[ "$MODE" == pending ]]; then
  echo
  echo "_Builds are still running; files are attached as each one finishes._"
elif [[ -f "$DIR/SHA256SUMS.txt" ]]; then
  echo
  echo "Checksums: \`SHA256SUMS.txt\`."
fi
