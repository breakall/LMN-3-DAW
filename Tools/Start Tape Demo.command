#!/bin/bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
emulator_repo="${LMN3_EMULATOR_DIR:-$(dirname "$repo")/LMN-3-Emulator}"
emulator="$emulator_repo/build/macos/LMN-3-Emulator_artefacts/Release/LMN-3-Emulator.app"
daw="$repo/build/macos/LMN-3_artefacts/Release/LMN-3.app"
session="$repo/Tape Demo"
if [[ ! -d "$emulator" || ! -d "$daw" ]]; then
    echo "Build both projects first: bash Tools/build-macos.sh in each repository."
    read -r -p "Press Return to close. "
    exit 1
fi
python3 "$repo/Tools/create_demo_phrase.py" "$session/demo-phrase.wav"
open "$emulator"
sleep 2
open "$daw" --args "--edit-file=$session/edit" --demo
