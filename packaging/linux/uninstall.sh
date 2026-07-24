#!/bin/sh
# Removes what packaging/linux/install.sh installed (system-wide files — needs
# root, run with sudo).
set -eu

VST3="/usr/lib/vst3/Anamorph.vst3"
APP="/usr/local/bin/Anamorph"

[ "$(id -u)" -eq 0 ] || { echo "error: removing a system-wide install needs root — run:  sudo ./uninstall.sh" >&2; exit 1; }

removed=0
if [ -d "$VST3" ]; then rm -rf "$VST3"; echo "removed $VST3"; removed=1; fi
if [ -f "$APP" ];  then rm -f  "$APP";  echo "removed $APP";  removed=1; fi

[ "$removed" -eq 1 ] || echo "nothing to remove (already uninstalled?)"
echo "Per-user presets/settings (if any) are kept; remove them manually if desired."
