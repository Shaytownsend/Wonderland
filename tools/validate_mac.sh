#!/usr/bin/env bash
# M8 validation on macOS (run from the repo root after a Release build).
#   1. ad-hoc codesign + auval      2. pluginval strictness 8 (AU + VST3)      3. Standalone CPU via top
set -euo pipefail
ART="build/TheQueen_artefacts/Release"
AU="$HOME/Library/Audio/Plug-Ins/Components/The Queen.component"
VST3="$HOME/Library/Audio/Plug-Ins/VST3/The Queen.vst3"
[ -d "$AU" ] || AU="$ART/AU/The Queen.component"
[ -d "$VST3" ] || VST3="$ART/VST3/The Queen.vst3"

echo "== codesign (ad-hoc)"
codesign --force --deep -s - "$AU"
codesign --force --deep -s - "$VST3"

echo "== auval"
if ! auval -v aumu Quen Lshp | tee /tmp/queen_auval.log | tail -3; then :; fi
if ! grep -q "AU VALIDATION SUCCEEDED" /tmp/queen_auval.log; then
    echo "auval did not find/validate the plugin — restarting AudioComponentRegistrar and retrying"
    killall -9 AudioComponentRegistrar 2>/dev/null || true
    sleep 2
    auval -v aumu Quen Lshp | tee /tmp/queen_auval.log | tail -3
fi

echo "== pluginval"
if [ ! -x tools/pluginval.app/Contents/MacOS/pluginval ]; then
    curl -sSL -o /tmp/pluginval_macOS.zip https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_macOS.zip
    ditto -x -k /tmp/pluginval_macOS.zip tools/
fi
tools/pluginval.app/Contents/MacOS/pluginval --strictness-level 8 --validate "$AU"
tools/pluginval.app/Contents/MacOS/pluginval --strictness-level 8 --validate "$VST3"

echo "== Standalone CPU: hold 6 notes on the on-screen keyboard (or MIDI) at 48 kHz / 128, then press return"
open "$ART/Standalone/The Queen.app"
read -r _
PID=$(pgrep -f "The Queen.app/Contents/MacOS" | head -1)
top -pid "$PID" -l 11 -s 1 -stats cpu | awk '/^[0-9.]+$/ {n++; if (n>1) {s+=$1; c++}} END {printf "mean CPU over %d s: %.1f %%\n", c, s/c}'
