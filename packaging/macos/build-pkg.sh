#!/bin/bash
# Builds the Anamorph macOS installer package (.pkg) from the CI-staged
# customer payload (the RH-PR-6 installer; unsigned until RH-PR-3 adds
# Developer ID signing + notarization of this same package).
#
#   build-pkg.sh <staged-dir> <version> <output.pkg>
#
# <staged-dir> must contain Anamorph.vst3 / Anamorph.component / Anamorph.app
# exactly as validated by the build.yml package step (stripped, universal,
# ad-hoc signed). pkgbuild archives the payload as-is — permissions and the
# signed bundle layout are preserved. Files installed by Installer.app carry
# no quarantine attribute, so a pkg install needs no xattr step afterwards
# (unlike the zip, whose extracted bundles inherit quarantine).
# The distribution offers COMPONENT SELECTION (VST3 / AU / Standalone app,
# all pre-selected — the default is a full install; Installer.app's
# "Customize" button exposes the checkboxes).
set -euo pipefail

DIST=${1:?usage: build-pkg.sh <staged-dir> <version> <output.pkg>}
VERSION=${2:?usage: build-pkg.sh <staged-dir> <version> <output.pkg>}
OUT=${3:?usage: build-pkg.sh <staged-dir> <version> <output.pkg>}

for b in Anamorph.vst3 Anamorph.component Anamorph.app; do
  [ -d "$DIST/$b" ] || { echo "error: $DIST/$b missing (staged payload incomplete)" >&2; exit 1; }
done

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# One component package per install destination.
mkdir -p "$WORK/vst3" "$WORK/au" "$WORK/app"
cp -R "$DIST/Anamorph.vst3"      "$WORK/vst3/"
cp -R "$DIST/Anamorph.component" "$WORK/au/"
cp -R "$DIST/Anamorph.app"       "$WORK/app/"

pkgbuild --root "$WORK/vst3" --identifier com.rollytech.anamorph.vst3 \
         --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/VST3" \
         "$WORK/AnamorphVST3.pkg"
pkgbuild --root "$WORK/au"   --identifier com.rollytech.anamorph.au \
         --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/Components" \
         "$WORK/AnamorphAU.pkg"
pkgbuild --root "$WORK/app"  --identifier com.rollytech.anamorph.app \
         --version "$VERSION" --install-location "/Applications" \
         "$WORK/AnamorphApp.pkg"

# Distribution definition, written explicitly (a synthesized one hard-wires
# customize="never", which hides the component checkboxes). customize="allow"
# keeps the default flow a FULL install (all three choices start selected)
# while the Installer's "Customize" button lets the user deselect components.
# enable_localSystem pins the system-wide destinations (/Library, /Applications).
cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Anamorph ${VERSION}</title>
    <options customize="allow" require-scripts="false"/>
    <domains enable_localSystem="true"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
        <line choice="app"/>
    </choices-outline>
    <choice id="vst3" title="VST3 Plug-in" start_selected="true"
            description="Installs Anamorph.vst3 into /Library/Audio/Plug-Ins/VST3 (REAPER, Ableton Live, Cubase, Bitwig, ...).">
        <pkg-ref id="com.rollytech.anamorph.vst3"/>
    </choice>
    <choice id="au" title="AU Plug-in" start_selected="true"
            description="Installs Anamorph.component into /Library/Audio/Plug-Ins/Components (Logic Pro, GarageBand, ...).">
        <pkg-ref id="com.rollytech.anamorph.au"/>
    </choice>
    <choice id="app" title="Standalone Application" start_selected="true"
            description="Installs Anamorph.app into /Applications.">
        <pkg-ref id="com.rollytech.anamorph.app"/>
    </choice>
    <pkg-ref id="com.rollytech.anamorph.vst3" version="${VERSION}">AnamorphVST3.pkg</pkg-ref>
    <pkg-ref id="com.rollytech.anamorph.au" version="${VERSION}">AnamorphAU.pkg</pkg-ref>
    <pkg-ref id="com.rollytech.anamorph.app" version="${VERSION}">AnamorphApp.pkg</pkg-ref>
</installer-gui-script>
EOF
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" "$OUT"

# Self-check: the package must expand, contain all three components, and keep
# component selection enabled (customize="allow" with every choice pre-selected
# — i.e. the default remains a full install). `installer -pkginfo` is a query
# flag (no root needed) — validated on the macos-14 runner (CI run
# 30056361865); installing is what needs sudo, not this.
installer -pkginfo -pkg "$OUT"
pkgutil --expand "$OUT" "$WORK/expanded"
for id in vst3 au app; do
  grep -Rq "com.rollytech.anamorph.$id" "$WORK/expanded" \
    || { echo "error: component com.rollytech.anamorph.$id missing from $OUT" >&2; exit 1; }
done
grep -q 'customize="allow"' "$WORK/expanded/Distribution" \
  || { echo "error: $OUT lost customize=\"allow\" (component selection disabled)" >&2; exit 1; }
[ "$(grep -c 'start_selected="true"' "$WORK/expanded/Distribution")" -eq 3 ] \
  || { echo "error: $OUT does not pre-select all three components (default must be a full install)" >&2; exit 1; }
echo "built $OUT"
