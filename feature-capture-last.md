Feature Plan: Capture Last N Bars

Goal
- Add a "Capture Last" action that records a rolling buffer and commits the last N bars to a new audio clip aligned to the bar grid.

Scope (MVP)
- Capture source: master output (simpler than per-track).
- Buffer size: user-selectable 1/2/4/8 bars (max 8).
- Quantization: align to bar start based on current transport position and tempo at capture time.
- Output: create a new audio clip on a target track (new track by default).
- Capture only while transport is playing (disabled when stopped).

Resolved decisions
- Audio tap: use a master-track tap plugin for reliable post-mix capture.
- Threading: copy the ring buffer into a snapshot on capture, then write WAV off the audio thread.
- Undo/redo: MVP does not add undo integration; revisit after feature stabilizes.
- Tempo changes: use current tempo at capture time; no dynamic tempo tracking in buffer.
- Sample rate changes: reinitialize ring buffer on device change.
- Storage: write under temp capture directory; no media pool promotion in MVP.
- UI placement: add controls near the existing Render button in `EditTabBarView`.
- Channels: capture stereo (first two outputs); ignore extra channels.

User Flow
1) User selects bar length (1/2/4/8).
2) User presses "Capture Last".
3) DAW creates a clip containing the last N bars from the rolling buffer.

Architecture Plan
1) Add a rolling audio buffer service
- New module: `Source/Modules/app_services/CaptureLastService.{h,cpp}`.
- API sketch:
  - `setBars(int bars)`, `setSampleRate(double)`, `setNumChannels(int)`.
  - `pushOutputBlock(const float** channels, int numChannels, int numSamples)`.
  - `captureToFile(const tracktion::Edit&, juce::File outputFile, CaptureRange range)`.
- Ring buffer fields: `juce::AudioBuffer<float> ring`, `int writePos`, `int totalSamples`, `int numChannels`.
- Buffer size = `maxBars * beatsPerBar * (60.0 / bpm) * sampleRate`.

2) Tap the master output (resolved)
- Implement `CaptureTapPlugin` under `Source/Modules/internal_plugins/CaptureTapPlugin`.
- Insert it on the master track at startup (post-fx position).
- In `processBlock`, copy the output buffer to `CaptureLastService::pushOutputBlock`.
- Keep the plugin allocation-free and guard with enable/disable flag.

3) Capture action -> write file -> insert clip
- UI entry point: add a "Capture Last" action near the render flow in `Source/Views/Edit/EditTabBarView.cpp`.
- Compute capture range:
  - Get transport position `edit.getTransport().getPosition()`.
  - Use `edit.tempoSequence` to find the current bar boundaries.
  - Calculate `barEnd = floorToBar(position)`, `barStart = barEnd - bars`.
- Extract ring buffer slice aligned to `[barStart, barEnd]` and write to WAV:
  - Use `juce::WavAudioFormat` + `juce::AudioFormatWriter`.
  - Write on message/background thread (not the audio thread).
- Insert new audio clip:
  - Target track: create a new `AudioTrack` or use a selected track.
  - `track->insertNewClip(tracktion::TrackItem::Type::audio, "capture", timeRange, nullptr);`
  - Set the clip's file/AudioFile reference to the captured WAV (confirm API, likely `tracktion::AudioClip::setSourceFile`).

4) State and storage
- Store captured files under `engine.getTemporaryFileManager().getTempDirectory()/captures`.
- Optionally promote to project media pool on save (defer to later).

5) UI wiring & MIDI
- Add UI controls: "Capture Last" button + bar length (1/2/4/8) selector.
- Optional MIDI mapping: add a new button in `MidiCommandManager` and forward to `EditTabBarView`.

Risks / Constraints
- Tempo changes mid-buffer: MVP can assume constant tempo (use current tempo at capture time).
- Latency: capture tap is post-mix via plugin to match playback timing.
- Disk IO: write file on UI thread or background thread (not audio thread).
- If the plugin is bypassed/disabled, buffer stops updating.

Milestones
1) Buffer + tap audio (rolling buffer works).
2) Capture command writes WAV.
3) Clip insertion aligned to bars.
4) UI control for capture + bars.

Testing
- Capture while playing: verify clip aligns to bar and plays back correctly.
- Capture at different bar lengths.
- Capture at different tempos.
- Device change: update buffer sizing on sample rate change.
