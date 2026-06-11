#!/usr/bin/env bash
# Install the built (and ad-hoc-signed) plugins into ~/Library/Audio/Plug-Ins.
#
# Refuses to run while Ableton Live is open: Live never reloads a loaded
# .vst3 module, and swapping the files underneath it leaves new device
# instances in a broken half-state (their MIDI-output tap disappears from
# the routing choosers). Quit Live, install, relaunch.
set -euo pipefail
cd "$(dirname "$0")/.."

red()   { printf '\033[1;31m%s\033[0m\n' "$*"; }
green() { printf '\033[1;32m%s\033[0m\n' "$*"; }

if pgrep -x Live >/dev/null 2>&1; then
    red "error: Ableton Live is running - quit it first."
    red "       (Live keeps the loaded module; swapping the bundle under it"
    red "        breaks MIDI routing for newly inserted instances)"
    exit 1
fi

VST3_SRC="build/VirtualFret_artefacts/Release/VST3/VirtualFret.vst3"

[ -d "$VST3_SRC" ] || { red "error: $VST3_SRC not found - build first"; exit 1; }

mkdir -p ~/Library/Audio/Plug-Ins/VST3
rm -rf ~/Library/Audio/Plug-Ins/VST3/VirtualFret.vst3
cp -R "$VST3_SRC" ~/Library/Audio/Plug-Ins/VST3/

# Re-seal in place: a plain `cmake --build` leaves only the linker's
# ad-hoc signature on the binary, with no bundle seal.
# (--timestamp=none is the ad-hoc convention.)
codesign --sign - --deep --force --timestamp=none ~/Library/Audio/Plug-Ins/VST3/VirtualFret.vst3
codesign --verify ~/Library/Audio/Plug-Ins/VST3/VirtualFret.vst3
green "ok: VirtualFret.vst3 -> ~/Library/Audio/Plug-Ins/VST3/"
