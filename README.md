# LMN-3-DAW
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

This repository contains the code for the LMN-3-DAW.

If you would like to support the project, please consider becoming a [sponsor](https://github.com/sponsors/stonepreston).

## Overview Video
An overview video showcasing the project can be viewed [here](https://www.youtube.com/watch?v=h5UmPTttN1s)

## Releases
Official releases can be found [here](https://github.com/FundamentalFrequency/LMN-3-DAW/releases). A release contains executables for
x86_64, ARM, and aarch64 architectures. The aarch64 bit version of the application is recommended for running on the Pi.
Note that this does require you to run the 64-Bit version of the Raspberry Pi OS if you are not already.

There is an other repository with community releases, you can find at [iamdey/LMN-3-DAW](https://github.com/iamdey/LMN-3-DAW/releases).
Look at https://github.com/orgs/FundamentalFrequency/discussions/124 for more informations.

### Install or Update

Follow the download section of the [build guide](https://github.com/FundamentalFrequency/LMN-3-Build-Guide?tab=readme-ov-file#downloading-the-lmn-3-daw).

## Configuration
If you wish to configure the application, you can add a `config.yaml` file to `~/.config/LMN-3`.
The only configuration currently supported is whether to show a title bar, and the width and height
of the application window. You can also configure a basic color scheme. An example config file is shown below:
```yaml
config:
  show-title-bar: false
  size:
    width: 800
    height: 480
  colours:
    backgroundColour: "ff1d2021"
    textColour: "fff9f5d7"
    colour1: "ff458588"
    colour2: "ff689d6a"
    colour3: "fff9f5d7"
    colour4: "ffcc241d"
    colour5: "ff98971a"
    colour6: "ffd65d0e"
    colour7: "ffb16286"
    colour8: "ffd79921"
```

The first time you run the application, the directories `~/.config/LMN-3/samples` and
`~/.config/LMN-3/drum kits` will be automatically created. See the sections below for details on how to add
synth samples and drum kits to the application.

### Tape screen

The Tracks screen is a tape deck with animated reels, a playhead and four
waveform lanes. Blue highlights the selected track; the audio section under the
playhead has a light outline. The reels turn during playback and recording,
respond to position changes while stopped, and rest when the tape stops. The
header shows transport time, the selected track and any lifted section.

The four encoder cards show their current actions and values. Holding Control
changes the labels to the alternate actions:

| Encoder | Turn | Hold Control and turn | Push |
| --- | --- | --- | --- |
| Blue / 1 | Select track | Select track | Focus / four-lane view |
| Green / 2 | Move loop out | Move loop in | — |
| White / 3 | Move tape position | Move between beats | Solo |
| Orange / 4 | Zoom in / out | Change track colour | Mute |

Projects can still contain more than four tracks. Selecting a track beyond the
visible group pages to the next four lanes. Orange changes the visible time
range from 0.5 to 120 seconds and adjusts the fine-position increment with it.

The tape layout targets the LMN-3's 4-inch HyperPixel at **800 × 480**.
Typography follows `SimpleListItemView` and `ListTitle`: fonts occupy 70% of
their text row height. The transport uses the menus' 56-pixel scale, track numbers
use approximately 36 pixels, and the four encoder cards use 28-pixel labels
with 36-pixel values in two spacious rows. Secondary status and ruler text use
24 pixels. The focused view keeps the same readable track numbers.

The selected track's full name appears above the transport. An 80-pixel gutter
shows large track numbers and mute/solo badges, leaving 704 pixels for waveforms
at 800 × 480. Names are not repeated beside each lane. The timeline starts at
zero, so a clip beginning at zero sits directly beside the gutter. The playhead
moves right until it reaches the centre; the timeline then follows playback. Overview waveforms only show clip names
on the selected track.
Persistent fine-print instructions have been removed; recording and error
messages appear in the status strip. Control still shows the alternate encoder
actions. Encoder time values show tenths; the transport retains milliseconds.
The same gutter also fits 640 × 480 without shrinking the track numbers.

### Record a synth phrase to tape

On the Tracks screen, select a track with a synth or sampler instrument. Hold
**Control** and press **Record**, then play the keyboard. This records the
selected track's sound in real time to a new stereo audio track named after the
source (for example, `Synth Tape`). Ordinary **Record** still records MIDI.

Press **Stop** to finish. The tape track is selected automatically and its
waveform appears on the timeline. The original synth remains available on its
own track; the tape take contains audio rather than another MIDI performance.

- Turn **encoder 3** to position the playhead. **Control + encoder 3** moves
  between beats.
- Press **Slice** to split the audio at the playhead.
- Position the playhead inside a section and press **Cut** to lift it. At an
  exact split boundary, Cut selects the section to the right.
- Move the playhead to the destination and press **Paste** to drop it. Paste
  keeps the section's source audio and advances the playhead by its length;
  press it again to repeat the section. You can also choose another track
  before dropping.
- Press **Undo** to undo the last split, lift or drop. Press **Save** to save
  the arrangement. Undo history follows the existing Tracks-screen behavior.

Edits preserve the recording file. Dropping onto existing audio layers the
clips, so choose an empty destination if you want to move sections without
overlap. Recording captures everything sounding on the selected synth track,
including any MIDI clips already on it. Other tracks are not part of the tape
recording. For this first version, select the original synth again to record a
new take; each Control + Record creates another tape track.

To render the real screen with generated audio and exercise its controls on a
development machine, build the optional preview tool:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TAPE_PREVIEW=ON
cmake --build build --target TapeScreenPreview
./build/TapeScreenPreview_artefacts/Debug/TapeScreenPreview "$PWD/build/tape-screen-preview"
```

On a headless Linux machine, prefix the last command with `xvfb-run -a`.
The tool writes PNGs for stopped, playing, recording, lifted, zoomed, focused,
Control-held and second-page states, plus reel-animation frames.

### Samples
Place any audio sample files you wish to use with the Sampler plugin in `~/.config/LMN-3/samples`.

### Drum Kits
Drum kits are handled a bit differently than regular samples. Drum kits are essentially directories located in
`~/.config/LMN-3/drum_kits`, with each kit directory containing a `.yaml` mapping file along with any sample files
associated with it.

As an example, suppose I want to make a drum kit called AwesomeDrumKit. I would first create a directory in
`~/.config/LMN-3/drum_kits` named AwesomeDrumKit.
```
mkdir ~/.config/LMN-3/drum_kits/AwesomeDrumKit
```

I would then add all the samples I want to use in that kit into that
newly created AwesomeDrumKit folder (if you are doing this using the file browser you will need to make sure
and show hidden folders since `.config` is hidden.) Then I would create the yaml file:
```
touch  ~/.config/LMN-3/drum_kits/AwesomeDrumKit/AwesomeDrumKit.yaml
```
Notice that the `.yaml` file has the same name as its parent directory. You would then edit the file and define the mapping that states which sample is played by which midi note number:
```yaml
name: "AwesomeDrumKit"
mappings:
  - note_number: "53"
    file_name: "cool_sample.wav"
  - note_number: "54"
    file_name: "wacky_sample.wav"
  - note_number: "55"
    file_name: "crazy_sample.wav"
```

This is a manual process. 53 is the first note in the `+0` octave. You can add mappings for the entire note range from 5 to 124. If you want to make
a drum kit, you will need to create the directory to store the kit in, add the audio files to it, and then create the
`.yaml` mapping file. Perhaps someone could make a JUCE application to make this easier (cough...cough).

## Building From Source

### Dependencies
If you wish to build the project from source, you will also need to run the following command to install the build
dependencies:
```bash
sudo apt install libasound2-dev libjack-jackd2-dev \
    ladspa-sdk \
    libcurl4-openssl-dev  \
    libfreetype6-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
    libwebkit2gtk-4.0-dev \
    libglu1-mesa-dev mesa-common-dev \
    build-essential cmake clang llvm lld
```

### Cloning the Repository
This repository relies on git submodules. You must clone the submodules as well using:
```bash
git clone --recurse-submodules https://github.com/FundamentalFrequency/LMN-3-DAW
```

### Building the Project
CMake is used to build the project. Execute the following commands to build all project targets:
```bash
cmake -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-fuse-ld=lld
cmake --build build -j8
```
You will find the corresponding artifacts in the build directory that was created using the commands above.
`./build/LMN3_artefacts/Release` will contain the main LMN-3 executable.

### Compiling for the Raspberry Pi
If you want to build the DAW for the Raspberry Pi, you will need to either build it on the Pi itself (takes a fair bit of time),
or cross compile it. Cross compilation is outside the scope of this document, but you can take a look at the
[release workflow](https://github.com/FundamentalFrequency/LMN-3-DAW/blob/master/.github/workflows/release.yaml) for insight on
how it works.

Note that if you do build on the Pi you will need at least 4 GB of RAM.

## Running the Test Suite
After building the project using the commands above, you can run the test suite by executing the following command:
```bash
./build/Tests/Tests_artefacts/Release/Tests
```

## LMN-3-Emulator
If you lack LMN-3 hardware with which to control the DAW (or just want a more convenient method for testing purposes),
you can use the [LMN-3-Emulator](https://github.com/FundamentalFrequency/LMN-3-Emulator) directly on your desktop. The emulator
sends the very  same MIDI commands as the actual LMN-3 hardware, though it is missing a few features.

## Contributing
Pull requests are welcome and very much appreciated. Please read the [contributing guide](https://github.com/FundamentalFrequency/LMN-3-DAW/blob/master/contributing.md)
before submitting a pull request.

## Thanks
This project would not be possible without the following free and open source projects:
- [JUCE](https://github.com/juce-framework/JUCE)
- [Tracktion Engine](https://github.com/Tracktion/tracktion_engine)
- [googletest](https://github.com/google/googletest)
- [yaml-cpp](https://github.com/jbeder/yaml-cpp)
- [FontAwesome](https://github.com/FortAwesome/Font-Awesome)
- [CMake](https://gitlab.kitware.com/cmake/cmake)
- [The LLVM Project](https://github.com/llvm/llvm-project)
- [Raspberry Pi](https://github.com/raspberrypi)

and many others.

## License
Licensed under the [GPL 3.0](https://www.gnu.org/licenses/gpl-3.0.en.html) license.

This repository also includes the [yaml-cpp](https://github.com/jbeder/yaml-cpp) library which requires the following
notice be included:

```
Copyright (c) 2008-2015 Jesse Beder.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

### macOS tape demo

The DAW and emulator are separate apps. Clone both with `--recurse-submodules`
into sibling directories named `LMN-3-DAW` and `LMN-3-Emulator`. With Xcode,
CMake, Ninja and Python 3 installed, run `bash Tools/build-macos.sh` in each
repository. The scripts patch the pinned JUCE 6 window snapshot implementation
for macOS 15+ SDKs without changing the dependency versions.

Double-click `Tools/Start Tape Demo.command` in the DAW repository. This opens
the emulator first, then the tape screen using a separate session in
`~/Library/Application Support/LMN-3/Tape Demo/`. The emulator's MIDI Output should read **LMN-3 Emulator (virtual)**.
The first track (**PLAY SYNTH**) contains FourOsc; the second (**TRY EDITING**)
contains an original generated audio phrase for immediate splitting and moving.
The demo starts without a metronome or count-in. Use Save to preserve edits.

Keep the emulator window focused to play notes with `A W S E D F T G Y H U J K`.
Use the mouse or trackpad to scroll the coloured encoders; double-click a knob
to push it. Hold the Mac's **Control** key for alternate encoder functions and
**Control + Record** for audio capture. Encoder 1 selects the track. Hover a
button for its label: the five editing buttons are **Cut, Paste, Slice, Save,
Undo** from left to right; the transport row begins **Control, Record, Play,
Stop**. Keep the tape screen visible while operating the emulator.

You can also launch the DAW directly with `--edit-file=/absolute/path/to/edit`
to use a separate session; add `--demo` to seed a new session with FourOsc and
an optional sibling `demo-phrase.wav`. Existing sessions are never reseeded.
