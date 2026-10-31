#!/usr/bin/env bash
# Builds "The Queen Installer.pkg" from a Release build (macOS only).
# Installs: AU, VST3, CLAP -> /Library/Audio/Plug-Ins/..., Standalone -> /Applications,
#           GUI art -> /Library/Application Support/The Queen/Assets.
# Bundles are ad-hoc signed (no Apple Developer account needed); see README for real signing.
set -euo pipefail
ART="${1:-build/TheQueen_artefacts/Release}"
OUT="${2:-packaging/Output}"
VERSION="$(cat VERSION)"
STAGE="$(mktemp -d)/root"

ASSETS="$STAGE/Library/Application Support/The Queen/Assets"
mkdir -p "$STAGE/Library/Audio/Plug-Ins/Components" "$STAGE/Library/Audio/Plug-Ins/VST3" \
         "$STAGE/Library/Audio/Plug-Ins/CLAP" "$STAGE/Applications" "$ASSETS" "$OUT"
# GUI art shared by all formats (the CI build is configured with -DQUEEN_EMBED_ASSETS=OFF)
cp assets/*.png "$ASSETS/"
cp -R "$ART/AU/The Queen.component" "$STAGE/Library/Audio/Plug-Ins/Components/"
cp -R "$ART/VST3/The Queen.vst3" "$STAGE/Library/Audio/Plug-Ins/VST3/"
cp -R "$ART/CLAP/The Queen.clap" "$STAGE/Library/Audio/Plug-Ins/CLAP/"
cp -R "$ART/Standalone/The Queen.app" "$STAGE/Applications/"

for b in "$STAGE"/Library/Audio/Plug-Ins/*/* "$STAGE/Applications/The Queen.app"; do
    codesign --force --deep -s - "$b"
done

# install exactly where stated, even if another copy of a bundle exists elsewhere
PLIST="$(dirname "$STAGE")/components.plist"
pkgbuild --analyze --root "$STAGE" "$PLIST"
i=0
while /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$PLIST" 2>/dev/null; do i=$((i + 1)); done
echo "bundles in package: $i"

pkgbuild --root "$STAGE" --component-plist "$PLIST" --identifier com.lanceshipp.thequeen.pkg \
         --version "$VERSION" --install-location / --scripts packaging/scripts \
         "$OUT/The Queen Installer.pkg"
echo "wrote $OUT/The Queen Installer.pkg"
