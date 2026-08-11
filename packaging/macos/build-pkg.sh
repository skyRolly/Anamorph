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
#
# Every component is built NON-RELOCATABLE and NON-VERSION-CHECKED, so each
# install copies the payload to its declared destination unconditionally —
# see build_component() below (INC-012).
set -euo pipefail

DIST=${1:?usage: build-pkg.sh <staged-dir> <version> <output.pkg>}
VERSION=${2:?usage: build-pkg.sh <staged-dir> <version> <output.pkg>}
OUT=${3:?usage: build-pkg.sh <staged-dir> <version> <output.pkg>}

for b in Anamorph.vst3 Anamorph.component Anamorph.app; do
  [ -d "$DIST/$b" ] || { echo "error: $DIST/$b missing (staged payload incomplete)" >&2; exit 1; }
done

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# Sets a component-plist key, adding it if pkgbuild's analysis did not emit it.
#   plist_put <plist> <array-index> <key> <type> <value>
plist_put () {
  /usr/libexec/PlistBuddy -c "Set :$2:$3 $5" "$1" 2>/dev/null \
    || /usr/libexec/PlistBuddy -c "Add :$2:$3 $4 $5" "$1"
}

# Builds one component package, with RELOCATION and VERSION CHECKING OFF.
#
# Why: pkgbuild's default component plist marks every bundle it finds as
# relocatable. At install time the Installer then looks the bundle identifier
# up in the system's receipt/Spotlight database and, if a copy is found
# ANYWHERE, writes the payload over THAT copy instead of the declared
# --install-location. Move /Applications/Anamorph.app to the Desktop (or drag
# it to the Trash, which is still on disk and still indexed) and re-run the
# installer: it reports success, updates the copy it found, and /Applications
# stays empty. Version checking fails the same way from the other end — a
# bundle already at or above the package version is skipped rather than
# overwritten. Both are switched off here, so a component always writes its
# payload to its declared destination, from the payload alone, with no
# reference to previous installation state (INC-012).
#
# BundleOverwriteAction=upgrade (pkgbuild's default, pinned explicitly) makes
# that write a replacement rather than a merge, so no file from an older
# install survives inside the new bundle.
build_component () {
  local id=$1 bundle=$2 dest=$3 out=$4
  local root="$WORK/root-$id" plist="$WORK/component-$id.plist" scripts="$WORK/scripts-$id"

  mkdir -p "$root" "$scripts"
  cp -R "$DIST/$bundle" "$root/"

  # Patch what pkgbuild itself analysed (rather than hand-writing the plist),
  # so RootRelativeBundlePath always matches and nested bundles are covered too.
  pkgbuild --analyze --root "$root" "$plist"
  local i=0
  while /usr/libexec/PlistBuddy -c "Print :$i:RootRelativeBundlePath" "$plist" >/dev/null 2>&1; do
    plist_put "$plist" "$i" BundleIsRelocatable    bool   false
    plist_put "$plist" "$i" BundleIsVersionChecked bool   false
    plist_put "$plist" "$i" BundleOverwriteAction  string upgrade
    i=$((i + 1))
  done
  [ "$i" -gt 0 ] || { echo "error: pkgbuild --analyze found no bundle in $bundle" >&2; exit 1; }

  # Fail-closed backstop: if the payload is not at the destination when the
  # component finishes, the install reports FAILURE instead of success. It runs
  # only for components the user actually selected.
  cat > "$scripts/postinstall" <<POST
#!/bin/sh
# Installed-state check for $bundle (\$3 = destination volume).
set -eu
DEST="\${3:-/}"
DEST="\${DEST%/}$dest/$bundle"
[ -e "\$DEST" ] || { echo "Anamorph: $bundle is missing from \$DEST after install" >&2; exit 1; }
exit 0
POST
  chmod +x "$scripts/postinstall"

  pkgbuild --root "$root" --identifier "com.rollytech.anamorph.$id" \
           --version "$VERSION" --install-location "$dest" \
           --component-plist "$plist" --scripts "$scripts" "$out"
}

# One component package per install destination.
build_component vst3 Anamorph.vst3      "/Library/Audio/Plug-Ins/VST3"       "$WORK/AnamorphVST3.pkg"
build_component au   Anamorph.component "/Library/Audio/Plug-Ins/Components" "$WORK/AnamorphAU.pkg"
build_component app  Anamorph.app       "/Applications"                      "$WORK/AnamorphApp.pkg"

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

# Install-state independence (INC-012): no component may be relocatable or
# version-checked, and each must carry its postinstall check. Empty elements are
# written self-closing (`<relocate/>`), so it is the `<relocate><bundle` pair
# that means a bundle is listed.
while IFS= read -r info; do
  flat=$(tr -d ' \n\t' < "$info")
  case "$flat" in
    *'<relocate><bundle'*)
      echo "error: $info marks a bundle relocatable — a re-install could write over a moved copy" >&2; exit 1 ;;
  esac
  case "$flat" in
    *'<version-check><bundle'*)
      echo "error: $info marks a bundle version-checked — a re-install could skip the destination" >&2; exit 1 ;;
  esac
  case "$flat" in
    *'<scripts><postinstall'*) ;;
    *) echo "error: $info has no postinstall installed-state check" >&2; exit 1 ;;
  esac
done < <(find "$WORK/expanded" -name PackageInfo)

# Payload completeness: every component must carry the whole bundle, so an
# install never depends on files a previous install left behind.
pkgutil --expand-full "$OUT" "$WORK/full"
while IFS='|' read -r pkg bundle; do
  [ -d "$WORK/full/$pkg/Payload/$bundle" ] \
    || { echo "error: $pkg carries no $bundle payload" >&2; exit 1; }
  [ -f "$WORK/full/$pkg/Payload/$bundle/Contents/MacOS/Anamorph" ] \
    || { echo "error: $pkg payload $bundle has no Contents/MacOS/Anamorph" >&2; exit 1; }
done <<'PAYLOADS'
AnamorphVST3.pkg|Anamorph.vst3
AnamorphAU.pkg|Anamorph.component
AnamorphApp.pkg|Anamorph.app
PAYLOADS
echo "built $OUT"
