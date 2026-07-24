#!/bin/sh
# Anamorph user-local installer (no root required).
# Installs the VST3 to ~/.vst3 and the Standalone to ~/.local/bin.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
VST3_SRC="$HERE/Anamorph.vst3"
APP_SRC="$HERE/Anamorph"
VST3_DIR="${HOME}/.vst3"
BIN_DIR="${HOME}/.local/bin"

[ -d "$VST3_SRC" ] || { echo "error: Anamorph.vst3 not found next to install.sh" >&2; exit 1; }
[ -f "$APP_SRC" ]  || { echo "error: Anamorph (Standalone) not found next to install.sh" >&2; exit 1; }

mkdir -p "$VST3_DIR" "$BIN_DIR"
rm -rf "$VST3_DIR/Anamorph.vst3"
cp -R "$VST3_SRC" "$VST3_DIR/"
cp "$APP_SRC" "$BIN_DIR/Anamorph"
chmod +x "$BIN_DIR/Anamorph" "$VST3_DIR/Anamorph.vst3/Contents/x86_64-linux/Anamorph.so" 2>/dev/null || true

echo "Installed:"
echo "  VST3       -> $VST3_DIR/Anamorph.vst3"
echo "  Standalone -> $BIN_DIR/Anamorph"
echo "Rescan plug-ins in your DAW to pick it up. Uninstall later with ./uninstall.sh"
case ":${PATH}:" in
  *":${BIN_DIR}:"*) ;;
  *) echo "note: ${BIN_DIR} is not on your PATH; add it to launch the Standalone by name." ;;
esac
