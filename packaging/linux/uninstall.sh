#!/bin/sh
# Removes what packaging/linux/install.sh installed (user-local files only).
set -eu

VST3="${HOME}/.vst3/Anamorph.vst3"
APP="${HOME}/.local/bin/Anamorph"
removed=0

if [ -d "$VST3" ]; then rm -rf "$VST3"; echo "removed $VST3"; removed=1; fi
if [ -f "$APP" ];  then rm -f  "$APP";  echo "removed $APP";  removed=1; fi

[ "$removed" -eq 1 ] || echo "nothing to remove (already uninstalled?)"
echo "User presets/settings (if any) are kept; remove them manually if desired."
