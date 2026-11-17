# Tape editing progress

This branch adds synth-to-audio recording, split/lift/drop editing, waveform
lanes, animated reels and colour-coded encoder controls to the Tracks screen.
Mac support uses the companion LMN-3-Emulator tape-editing branch.

Validation before the progress snapshot:

- Linux: application and tape preview built; 88 tests passed.
- Linux UI preview: recording, playback animation, paused reels, loop, zoom,
  focus and track paging exercised with actual rendered components.
- Apple Silicon Mac: application and emulator built with the current installed
  SDK; 88 native tests passed, including the audio recording/editing cases.
- Interactive Mac emulator testing is pending. A build or test pass alone does
  not establish that the live controller workflow or audio device choice works.

Run `bash Tools/build-macos.sh` in both repositories, then open
`Tools/Start Tape Demo.command`. See the README for controls. The JUCE changes
are captured by an idempotent source patch script; dependency commit pointers
remain pinned. No OP-1 firmware, extracted assets or decompiled code is included.
