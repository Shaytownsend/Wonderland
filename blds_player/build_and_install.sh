#!/usr/bin/env bash
# BLDS Player: configure, build (Release, universal), verify, sign, install, validate.
#
#   ./build_and_install.sh [--skip-pluginval] [--test-seconds N]
#
set -euo pipefail

cd "$(dirname "$0")"
ROOT="$(pwd)"
BUILD="$ROOT/build"
CONFIG=Release
TEST_SECONDS=600
RUN_PLUGINVAL=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-pluginval) RUN_PLUGINVAL=0 ;;
        --test-seconds)   TEST_SECONDS="$2"; shift ;;
        *) echo "Unknown option: $1"; exit 2 ;;
    esac
    shift
done

step() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }
die()  { printf '\n\033[1;31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }

[[ "$(uname -s)" == "Darwin" ]] || die "This script must run on macOS."
command -v cmake >/dev/null || die "cmake not found (brew install cmake)."
xcode-select -p >/dev/null 2>&1 || die "Xcode command line tools not installed (xcode-select --install)."

#-------------------------------------------------------------------------------
step "Checking blds_files/"
found=0
for rate in 44100 48000 88200 96000 176400 192000; do
    f="$ROOT/blds_files/BLDS_${rate}.wav"
    if [[ -f "$f" ]]; then
        echo "  found   BLDS_${rate}.wav"
        found=$((found + 1))
    else
        echo "  MISSING BLDS_${rate}.wav  (${rate} Hz will report 'No BLDS file')"
    fi
done
if [[ $found -eq 0 ]]; then
    echo
    echo "  WARNING: no official BLDS files found. The plugin will build but output silence."
    echo "           Generate them with DirectOut BLDS Generator v1.1 (see README.md)."
fi

#-------------------------------------------------------------------------------
# Pick a macOS SDK the installed linker can actually use. Some Command Line Tools
# installs default to an SDK newer than their own ld (e.g. "tapi error: unknown
# architecture arm64e.x1"); in that case fall back to the SDK matching this OS.
step "Checking the compiler/SDK"
probe_dir="$(mktemp -d)"
printf 'int main(void){return 0;}\n' > "$probe_dir/t.c"
can_link() {  # $1 = SDK path, or empty for the default
    local sysroot=()
    [[ -n "$1" ]] && sysroot=(-isysroot "$1")
    cc -arch arm64 -arch x86_64 ${sysroot[@]+"${sysroot[@]}"} "$probe_dir/t.c" -o "$probe_dir/t" >/dev/null 2>&1
}

if [[ -n "${SDKROOT:-}" ]]; then
    can_link "$SDKROOT" || die "SDKROOT=$SDKROOT cannot link a test program."
    echo "  using SDKROOT=$SDKROOT"
elif can_link ""; then
    echo "  default SDK works: $(xcrun --show-sdk-path 2>/dev/null)"
else
    os_major="$(sw_vers -productVersion | cut -d. -f1)"
    dev_dir="$(xcode-select -p)"
    for sdk in "$dev_dir"/SDKs/MacOSX"$os_major"*.sdk \
               "$dev_dir"/Platforms/MacOSX.platform/Developer/SDKs/MacOSX"$os_major"*.sdk \
               "$dev_dir"/SDKs/MacOSX*.sdk; do
        [[ -d "$sdk" ]] || continue
        if can_link "$sdk"; then
            export SDKROOT="$sdk"
            break
        fi
    done
    [[ -n "${SDKROOT:-}" ]] || die "No installed macOS SDK links with this linker. Reinstall Command Line Tools or install Xcode."
    echo "  default SDK is unusable with this linker; using $SDKROOT"
fi
rm -rf "$probe_dir"

SYSROOT_ARGS=()
if [[ -n "${SDKROOT:-}" ]]; then
    SYSROOT_ARGS=(-DCMAKE_OSX_SYSROOT="$SDKROOT")
    # A cache made with a different SDK (or a failed compiler check) must be discarded.
    if [[ -f "$BUILD/CMakeCache.txt" ]] && ! grep -q "^CMAKE_OSX_SYSROOT:.*=$SDKROOT\$" "$BUILD/CMakeCache.txt"; then
        echo "  discarding old CMake cache (different SDK)"
        rm -rf "$BUILD/CMakeCache.txt" "$BUILD/CMakeFiles"
    fi
fi

#-------------------------------------------------------------------------------
step "Configuring ($CONFIG, arm64 + x86_64, macOS 12+)"
# (No empty arrays: macOS bash 3.2 treats them as unbound under 'set -u'.)
if command -v ninja >/dev/null; then
    cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=$CONFIG ${SYSROOT_ARGS[@]+"${SYSROOT_ARGS[@]}"}
else
    cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=$CONFIG ${SYSROOT_ARGS[@]+"${SYSROOT_ARGS[@]}"}
fi

step "Building"
cmake --build "$BUILD" --config $CONFIG --parallel "$(sysctl -n hw.ncpu)"

#-------------------------------------------------------------------------------
step "Running bit-exact verification tests (${TEST_SECONDS}s per run)"
for t in BLDSPlayerTests BLDSPlayerFXTests; do
    exe="$(find "$BUILD" -type f -name "$t" -perm -u+x | head -n 1)"
    [[ -n "$exe" ]] || die "Test binary $t not found."
    echo "--- $t"
    "$exe" --seconds "$TEST_SECONDS" || die "$t FAILED - not installing."
done

#-------------------------------------------------------------------------------
ART_I="$BUILD/BLDSPlayer_artefacts/$CONFIG"
ART_E="$BUILD/BLDSPlayerFX_artefacts/$CONFIG"
BUNDLES=(
    "$ART_I/AU/BLDS Player.component"
    "$ART_I/VST3/BLDS Player.vst3"
    "$ART_E/AU/BLDS Player FX.component"
    "$ART_E/VST3/BLDS Player FX.vst3"
)

step "Checking universal binaries"
for b in "${BUNDLES[@]}"; do
    [[ -d "$b" ]] || die "Missing bundle: $b"
    bin="$b/Contents/MacOS/$(basename "${b%.*}")"
    archs="$(lipo -archs "$bin")"
    echo "  $(basename "$b"): $archs"
    [[ "$archs" == *arm64* && "$archs" == *x86_64* ]] || die "$(basename "$b") is not universal."
done

step "Ad-hoc code signing"
for b in "${BUNDLES[@]}"; do
    codesign --force --deep --sign - --timestamp=none "$b"
    codesign --verify --deep --strict "$b"
    echo "  signed $(basename "$b")"
done

#-------------------------------------------------------------------------------
COMP_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
step "Installing to $COMP_DIR and $VST3_DIR"
mkdir -p "$COMP_DIR" "$VST3_DIR"
for b in "${BUNDLES[@]}"; do
    case "$b" in
        *.component) dest="$COMP_DIR/$(basename "$b")" ;;
        *.vst3)      dest="$VST3_DIR/$(basename "$b")" ;;
    esac
    rm -rf "$dest"
    ditto "$b" "$dest"
    echo "  installed $dest"
done

# Make macOS rescan Audio Units so auval/Live see the new build.
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
sleep 2

#-------------------------------------------------------------------------------
run_auval() {
    # Separate 'local' lines: bash 3.2 can't reference one in the same statement.
    local au_type="$1"
    local au_sub="$2"
    local au_manu="$3"
    local log="$BUILD/auval_${au_type}_${au_sub}.log"
    step "auval -v $au_type $au_sub $au_manu"
    auval -v "$au_type" "$au_sub" "$au_manu" 2>&1 | tee "$log" | tail -n 25
    grep -q "AU VALIDATION SUCCEEDED" "$log" || die "auval failed for $au_type $au_sub $au_manu (full log: $log)"
}
run_auval aumu Blds Lnce   # instrument
run_auval aufx Blde Lnce   # effect variant

#-------------------------------------------------------------------------------
if [[ $RUN_PLUGINVAL -eq 1 ]]; then
    PV_DIR="$BUILD/tools"
    PV="$PV_DIR/pluginval.app/Contents/MacOS/pluginval"
    if [[ ! -x "$PV" ]]; then
        step "Downloading pluginval"
        mkdir -p "$PV_DIR"
        curl -fL --retry 3 -o "$PV_DIR/pluginval_macOS.zip" \
            https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip \
            || die "Could not download pluginval (re-run with --skip-pluginval to skip)."
        (cd "$PV_DIR" && unzip -oq pluginval_macOS.zip)
        xattr -dr com.apple.quarantine "$PV_DIR/pluginval.app" 2>/dev/null || true
    fi

    for b in "$VST3_DIR/BLDS Player.vst3" "$VST3_DIR/BLDS Player FX.vst3" \
             "$COMP_DIR/BLDS Player.component" "$COMP_DIR/BLDS Player FX.component"; do
        step "pluginval --strictness-level 10: $(basename "$b")"
        "$PV" --strictness-level 10 --validate-in-process --timeout-ms 900000 \
              --output-dir "$BUILD/pluginval-logs" --validate "$b" \
            || die "pluginval failed for $(basename "$b") (logs in $BUILD/pluginval-logs)"
    done
fi

#-------------------------------------------------------------------------------
cat <<NEXT

$(printf '\033[1;32m')All done: tests passed, plugins signed, installed and validated.$(printf '\033[0m')

Next steps (do this identically on BOTH Macs):
  1. Quit and reopen Ableton Live 12 (Settings > Plug-Ins > Rescan if it was open).
  2. Create a new MIDI track and put "BLDS Player" (Instruments > Plug-Ins > Lnce) on it.
     Nothing else on that track.
  3. In the plugin: Output Channel = Right only. Status must be green "BLDS OK - <rate> Hz".
  4. Track Audio To: your interface, output pair "31/32". Volume exactly 0.0 dB, pan centred.
  5. Make sure nothing else in the set is routed to output 32.
  6. In DirectOut GlobCon (Show page, Trigger column) confirm BLDS is detected for this input.
See README.md for details.
NEXT
