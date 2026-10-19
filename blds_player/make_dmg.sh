#!/usr/bin/env bash
# Packages the already-built, tested and signed plugins into a DMG with a
# double-click installer, for installing on another Mac without building.
#
#   ./build_and_install.sh   (first, on this Mac)
#   ./make_dmg.sh            -> BLDS-Player-<version>.dmg
#
set -euo pipefail

cd "$(dirname "$0")"
ROOT="$(pwd)"
BUILD="$ROOT/build"
CONFIG=Release

die() { printf '\n\033[1;31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }

[[ "$(uname -s)" == "Darwin" ]] || die "This script must run on macOS."

VERSION="$(sed -n 's/^project(BLDSPlayer VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
[[ -n "$VERSION" ]] || VERSION="1.0.0"

BUNDLES=(
    "$BUILD/BLDSPlayer_artefacts/$CONFIG/AU/BLDS Player.component"
    "$BUILD/BLDSPlayer_artefacts/$CONFIG/VST3/BLDS Player.vst3"
    "$BUILD/BLDSPlayerFX_artefacts/$CONFIG/AU/BLDS Player FX.component"
    "$BUILD/BLDSPlayerFX_artefacts/$CONFIG/VST3/BLDS Player FX.vst3"
)

for b in "${BUNDLES[@]}"; do
    [[ -d "$b" ]] || die "Missing $(basename "$b"). Run ./build_and_install.sh first."
    codesign --verify --deep --strict "$b" 2>/dev/null \
        || die "$(basename "$b") is not signed. Run ./build_and_install.sh first."
done

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
STAGE="$WORK/BLDS Player $VERSION"
mkdir -p "$STAGE/Plug-Ins"

for b in "${BUNDLES[@]}"; do
    ditto "$b" "$STAGE/Plug-Ins/$(basename "$b")"
done

#-------------------------------------------------------------------------------
cat > "$STAGE/Install BLDS Player.command" <<'INSTALL'
#!/bin/bash
# Installs BLDS Player (AU + VST3, instrument + FX) for the current user.
HERE="$(cd "$(dirname "$0")" && pwd)"
COMP="$HOME/Library/Audio/Plug-Ins/Components"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"

echo "Installing BLDS Player..."
mkdir -p "$COMP" "$VST3" || { echo "Could not create plug-in folders."; read -r -p "Press Return to close."; exit 1; }

ok=1
for b in "$HERE"/Plug-Ins/*.component "$HERE"/Plug-Ins/*.vst3; do
    case "$b" in
        *.component) dest="$COMP/$(basename "$b")" ;;
        *.vst3)      dest="$VST3/$(basename "$b")" ;;
    esac
    rm -rf "$dest"
    if ditto "$b" "$dest"; then
        # Files copied from a downloaded/AirDropped DMG are quarantined; Live
        # would refuse to load them, so clear the flag on the installed copies.
        xattr -dr com.apple.quarantine "$dest" 2>/dev/null
        echo "  installed $dest"
    else
        echo "  FAILED    $dest"; ok=0
    fi
done

# Make macOS rescan Audio Units.
killall -9 AudioComponentRegistrar >/dev/null 2>&1
sleep 2

echo
echo "Checking the Audio Units with auval..."
for id in "aumu Blds Lnce" "aufx Blde Lnce"; do
    if auval -v $id 2>&1 | grep -q "AU VALIDATION SUCCEEDED"; then
        echo "  PASS  $id"
    else
        echo "  FAIL  $id   (run 'auval -v $id' in Terminal for details)"; ok=0
    fi
done

echo
if [ $ok -eq 1 ]; then
    echo "Done. Quit and reopen Ableton Live 12, then add BLDS Player"
    echo "(Plug-Ins > Lnce) to a MIDI track. See README.txt for the Live setup."
else
    echo "Something failed - see the messages above."
fi
echo
read -r -p "Press Return to close this window."
INSTALL

cat > "$STAGE/Uninstall BLDS Player.command" <<'UNINSTALL'
#!/bin/bash
rm -rf "$HOME/Library/Audio/Plug-Ins/Components/BLDS Player.component" \
       "$HOME/Library/Audio/Plug-Ins/Components/BLDS Player FX.component" \
       "$HOME/Library/Audio/Plug-Ins/VST3/BLDS Player.vst3" \
       "$HOME/Library/Audio/Plug-Ins/VST3/BLDS Player FX.vst3"
killall -9 AudioComponentRegistrar >/dev/null 2>&1
echo "BLDS Player removed."
read -r -p "Press Return to close this window."
UNINSTALL

chmod +x "$STAGE/Install BLDS Player.command" "$STAGE/Uninstall BLDS Player.command"

#-------------------------------------------------------------------------------
# README with the checksums of the embedded BLDS files, so both Macs can be
# confirmed to play byte-identical signals.
{
    echo "BLDS Player $VERSION"
    echo "Built $(date '+%Y-%m-%d %H:%M') on $(scutil --get ComputerName 2>/dev/null || hostname)"
    echo
    echo "INSTALL"
    echo "  1. Right-click (or Control-click) \"Install BLDS Player.command\" and choose Open."
    echo "     (A plain double-click may be blocked by macOS the first time, because the"
    echo "     installer isn't from an identified developer. Choose Open in the dialog.)"
    echo "     If macOS still refuses (\"cannot be opened\"), open Terminal and run:"
    echo "       bash \"/Volumes/BLDS Player $VERSION/Install BLDS Player.command\""
    echo "  2. Wait for PASS on both auval checks, then press Return."
    echo "  3. Quit and reopen Ableton Live 12."
    echo
    echo "ABLETON LIVE SETUP (identical on both Macs)"
    echo "  - New MIDI track, put BLDS Player on it (Plug-Ins > Lnce). Nothing else on the track."
    echo "  - Output Channel = Right only. Status must be green \"BLDS OK - <rate> Hz\"."
    echo "  - Track Audio To: your interface, output pair 31/32."
    echo "  - Track volume exactly 0.0 dB, pan centred. Nothing else routed to output 32."
    echo "  - Confirm in DirectOut GlobCon (Show page, Trigger column) that BLDS is detected."
    echo
    echo "EMBEDDED BLDS FILES (sha256)"
    found=0
    for f in "$ROOT"/blds_files/BLDS_*.wav; do
        [[ -f "$f" ]] || continue
        found=1
        echo "  $(shasum -a 256 "$f" | cut -d' ' -f1)  $(basename "$f")"
    done
    [[ $found -eq 1 ]] || echo "  (blds_files/ was empty when this DMG was made)"
    echo
    echo "Requires macOS 12 or later (Apple silicon or Intel)."
    echo "To remove: right-click \"Uninstall BLDS Player.command\" and choose Open."
} > "$STAGE/README.txt"

#-------------------------------------------------------------------------------
DMG="$ROOT/BLDS-Player-$VERSION.dmg"
rm -f "$DMG"
hdiutil create -volname "BLDS Player $VERSION" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null \
    || die "hdiutil could not create the DMG."
hdiutil verify "$DMG" >/dev/null || die "The DMG failed verification."

printf '\n\033[1;32mCreated %s (%s)\033[0m\n' "$DMG" "$(du -h "$DMG" | cut -f1 | tr -d ' ')"
echo "Copy it to the other Mac (AirDrop, USB stick...), open it, and follow README.txt."
open -R "$DMG"
