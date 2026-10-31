#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
AU=$(find build -type d -name 'Caris Throw Matrix.component' -print -quit 2>/dev/null || true)
VST=$(find build -type d -name 'Caris Throw Matrix.vst3' -print -quit 2>/dev/null || true)
if [[ -z "$AU" || -z "$VST" ]]; then echo 'No AU and VST3 found. Run ./build-macos.sh first.'; exit 1; fi
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components" "$HOME/Library/Audio/Plug-Ins/VST3"
# Install in user Library; no sudo needed, no modifications to any other plugin.
ditto "$AU" "$HOME/Library/Audio/Plug-Ins/Components/Caris Throw Matrix.component"
ditto "$VST" "$HOME/Library/Audio/Plug-Ins/VST3/Caris Throw Matrix.vst3"
codesign --force --deep --sign - "$HOME/Library/Audio/Plug-Ins/Components/Caris Throw Matrix.component" 2>/dev/null || true
codesign --force --deep --sign - "$HOME/Library/Audio/Plug-Ins/VST3/Caris Throw Matrix.vst3" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo 'Installed and ad-hoc signed Caris Throw Matrix v0.2 AU + VST3 for the current user.'
echo 'Fully quit Logic Pro, reopen it, and load Caris Throw Matrix (Mono->Stereo or Stereo).'
