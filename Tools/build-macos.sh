#!/bin/bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$repo/Tools/patch_macos_juce.py" "$repo/tracktion_engine/modules/juce"
cmake -S "$repo" -B "$repo/build/macos" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build "$repo/build/macos" --target LMN-3 Tests -j6
