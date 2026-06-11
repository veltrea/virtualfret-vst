#!/bin/bash
# Packages the Release artefacts into an ad-hoc-signed distributable ZIP.
#
# Following the signing approach established in FloatingMacro/scripts/build-app.sh:
#   codesign --sign - --deep --force --timestamp=none
# (--timestamp=none: pointless to contact Apple's timestamp server for ad-hoc.)
# After signing, verification uses the *shallow* `codesign --verify` — TCC and
# Gatekeeper check the main executable's seal; --deep --strict can emit noise
# about framework symlinks that does not matter.
#
# Usage:
#   bash scripts/package-release.sh            # assumes build/ already built
#   BUILD=1 bash scripts/package-release.sh    # cmake --build first

set -u -o pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
cd "$ROOT"

VERSION="$(grep -m1 'project(VirtualFret VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"
ART="$ROOT/build/VirtualFret_artefacts/Release"
DIST="$ROOT/dist"

say()  { printf '\033[1;36m== %s\033[0m\n' "$*"; }
ok()   { printf '\033[1;32m   ok: %s\033[0m\n' "$*"; }
fail() { printf '\033[1;31m   FAIL: %s\033[0m\n' "$*"; exit 1; }

if [ "${BUILD:-0}" = "1" ]; then
    say "Building Release"
    cmake --build build --target VirtualFret_VST3 VirtualFret_Standalone --parallel 8 || fail "build"
fi

[ -d "$ART/VST3/VirtualFret.vst3" ] || fail "missing $ART/VST3/VirtualFret.vst3 (build first)"

say "Ad-hoc signing"
for bundle in "$ART/VST3/VirtualFret.vst3" "$ART/Standalone/VirtualFret.app"; do
    [ -e "$bundle" ] || { echo "   skip (not built): $bundle"; continue; }
    codesign --sign - --deep --force --timestamp=none "$bundle" 2>&1 | tail -1
    codesign --verify "$bundle" || fail "signature verify: $bundle"
    ok "$(basename "$bundle")"
done

say "Generating dSYMs (crash symbolication)"
rm -rf "$DIST"
mkdir -p "$DIST/dSYM"
for bundle in "$ART/VST3/VirtualFret.vst3"; do
    [ -e "$bundle" ] || continue
    name="$(basename "$bundle")"
    dsymutil "$bundle/Contents/MacOS/VirtualFret" -o "$DIST/dSYM/${name}.dSYM" 2>/dev/null \
        && ok "${name}.dSYM" || echo "   warn: dsymutil failed for $name"
done

say "Packaging dist/VirtualFret-${VERSION}-macOS.zip"
mkdir -p "$DIST/VirtualFret-${VERSION}"
cp -R "$ART/VST3/VirtualFret.vst3" "$DIST/VirtualFret-${VERSION}/"
cp README.md README.ja.md "$DIST/VirtualFret-${VERSION}/"

(cd "$DIST" && ditto -c -k --keepParent "VirtualFret-${VERSION}" "VirtualFret-${VERSION}-macOS.zip") || fail "zip"
ok "$(du -sh "$DIST/VirtualFret-${VERSION}-macOS.zip" | awk '{print $1}')"

say "Done"
echo "   Install: copy VirtualFret.vst3 to ~/Library/Audio/Plug-Ins/VST3/"
echo "   If the DAW refuses to load: xattr -dr com.apple.quarantine <plugin path>"
