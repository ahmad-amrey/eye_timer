#!/usr/bin/env bash
# Packs "Eye Timer.app" into a drag-to-install disk image with a shortcut to
# /Applications. Users can drop the app there or anywhere else (for example
# ~/Applications, which needs no admin rights). Settings live in
# ~/Library/Application Support/EyeTimer, so replacing the app keeps them.
#
# Usage: packaging/macos/make-dmg.sh "build/Eye Timer.app" dist/EyeTimer-macos.dmg
set -euo pipefail

app="$1"
out="$2"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

cp -R "$app" "$stage/"
ln -s /Applications "$stage/Applications"
mkdir -p "$(dirname "$out")"
rm -f "$out"
hdiutil create -volname "Eye Timer" -srcfolder "$stage" -fs HFS+ -format UDZO -ov "$out"
echo "Disk image: $out"
