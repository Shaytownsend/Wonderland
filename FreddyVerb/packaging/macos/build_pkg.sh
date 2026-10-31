#!/usr/bin/env bash
# Builds "Freddy Verb <version>.pkg" from a Release build directory.
# Usage: packaging/macos/build_pkg.sh <build-dir> <version> [developer-id-installer-identity]
set -euo pipefail
BUILD="${1:?build dir}"; VERSION="${2:?version}"; SIGN_ID="${3:-}"
ART="$BUILD/FreddyVerb_artefacts/Release"
WORK="$(mktemp -d)"; OUT="$BUILD/installer"; mkdir -p "$OUT"

component() {   # name, source bundle, install location
  local name="$1" src="$2" dest="$3"
  [ -e "$src" ] || { echo "skip $name (not built)"; return; }
  mkdir -p "$WORK/$name/root"
  cp -R "$src" "$WORK/$name/root/"
  pkgbuild --root "$WORK/$name/root" --install-location "$dest" \
           --identifier "com.freddyaudio.freddyverb.$name" --version "$VERSION" "$WORK/$name.pkg"
  CHOICES+="<line choice=\"$name\"/>"
  OUTLINE+="<choice id=\"$name\" title=\"Freddy Verb $name\"><pkg-ref id=\"com.freddyaudio.freddyverb.$name\"/></choice><pkg-ref id=\"com.freddyaudio.freddyverb.$name\">$name.pkg</pkg-ref>"
}

CHOICES=""; OUTLINE=""
component vst3       "$ART/VST3/Freddy Verb.vst3"       "/Library/Audio/Plug-Ins/VST3"
component au         "$ART/AU/Freddy Verb.component"    "/Library/Audio/Plug-Ins/Components"
component clap       "$ART/CLAP/Freddy Verb.clap"       "/Library/Audio/Plug-Ins/CLAP"
component aax        "$ART/AAX/Freddy Verb.aaxplugin"   "/Library/Application Support/Avid/Audio/Plug-Ins"
component standalone "$ART/Standalone/Freddy Verb.app"  "/Applications"

cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>Freddy Verb $VERSION</title>
  <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <choices-outline>$CHOICES</choices-outline>
  $OUTLINE
</installer-gui-script>
XML

PKG="$OUT/Freddy Verb $VERSION.pkg"
if [ -n "$SIGN_ID" ]; then
  productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" --sign "$SIGN_ID" "$PKG"
else
  productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" "$PKG"
fi
echo "Built $PKG"
