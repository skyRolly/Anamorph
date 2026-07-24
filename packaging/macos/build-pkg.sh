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

# Distribution package: synthesize, then give the Installer window a title.
productbuild --synthesize \
  --package "$WORK/AnamorphVST3.pkg" \
  --package "$WORK/AnamorphAU.pkg" \
  --package "$WORK/AnamorphApp.pkg" \
  "$WORK/distribution.xml"
/usr/bin/sed -i '' \
  "s|<installer-gui-script minSpecVersion=\"[0-9.]*\">|&<title>Anamorph ${VERSION}</title>|" \
  "$WORK/distribution.xml"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" "$OUT"

# Self-check: the package must expand and contain all three components.
installer -pkginfo -pkg "$OUT"
pkgutil --expand "$OUT" "$WORK/expanded"
for id in vst3 au app; do
  grep -Rq "com.rollytech.anamorph.$id" "$WORK/expanded" \
    || { echo "error: component com.rollytech.anamorph.$id missing from $OUT" >&2; exit 1; }
done
echo "built $OUT"
