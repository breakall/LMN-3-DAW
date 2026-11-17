#!/bin/bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$repo/Tools/patch_macos_juce.py" "$repo/tracktion_engine/modules/juce"
patch_file="$repo/Tools/coreaudio-compat.patch"
if [[ -f "$repo/tracktion_engine/modules/juce/modules/juce_audio_devices/native/juce_CoreAudio_mac.cpp" ]]; then
    patch_file="$repo/Tools/coreaudio-modern-compat.patch"
fi
git -C "$repo/tracktion_engine/modules/juce" apply --reverse --check "$patch_file" 2>/dev/null || \
    git -C "$repo/tracktion_engine/modules/juce" apply "$patch_file"
cmake -S "$repo" -B "$repo/build/macos" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build "$repo/build/macos" --target LMN-3 Tests -j6
