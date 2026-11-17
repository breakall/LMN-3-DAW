// Renders the real tape components with generated audio and exercises controls.
#include "TapeStyle.h"
#include "TracksView.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

namespace {
void pump(int milliseconds) {
    juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
}

bool save(juce::Component &component, const juce::File &file) {
    file.deleteFile();
    juce::FileOutputStream stream(file);
    return stream.openedOk() &&
           juce::PNGImageFormat().writeImageToStream(
               component.createComponentSnapshot(component.getLocalBounds()),
               stream);
}

bool makeAudio(const juce::File &file, int voice) {
    juce::AudioBuffer<float> audio(2, 44100 * 6);
    for (int i = 0; i < audio.getNumSamples(); ++i) {
        const double t = i / 44100.0;
        const double beat = std::fmod(t, voice == 1 ? .8 : .4);
        const double envelope = std::exp(-beat * (voice == 2 ? 26 : 7));
        const double frequency = voice == 1 ? 55 : voice == 2 ? 147 : 220;
        const float sample = static_cast<float>(
            .6 * envelope * std::sin(t * frequency * 6.2831853) *
            (voice == 3 ? std::sin(t * 1.7) : 1));
        audio.setSample(0, i, sample);
        audio.setSample(1, i, sample * .8f);
    }
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(
        new juce::FileOutputStream(file), 44100, 2, 16, {}, 0));
    return writer &&
           writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
}

class AudioClock {
  public:
    explicit AudioClock(tracktion::HostedAudioDeviceInterface &audio)
        : thread([this, &audio] {
              juce::AudioBuffer<float> buffer(2, 256);
              juce::MidiBuffer midi;
              while (!stop.load()) {
                  buffer.clear();
                  midi.clear();
                  audio.processBlock(buffer, midi);
                  std::this_thread::sleep_for(std::chrono::microseconds(5805));
              }
          }) {}
    ~AudioClock() {
        stop = true;
        thread.join();
    }

  private:
    std::atomic<bool> stop{false};
    std::thread thread;
};
} // namespace

int main(int argc, char **argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc != 2) {
        std::cerr << "Usage: TapeScreenPreview /absolute/output/directory\n";
        return 1;
    }
    const juce::File output(argv[1]);
    if (output.createDirectory().failed())
        return 1;
    const auto temporary =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("lmn3-tape-screen", "", false);
    temporary.createDirectory();
    int result = 0;
    {
        tracktion::Engine engine{"Tape screen preview"};
        auto &audio = engine.getDeviceManager().getHostedAudioDeviceInterface();
        tracktion::HostedAudioDeviceInterface::Parameters parameters;
        parameters.inputChannels = 0;
        parameters.outputChannels = 2;
        parameters.blockSize = 256;
        parameters.fixedBlockSize = true;
        audio.initialise(parameters);
        audio.prepareToPlay(44100, 256);
        auto edit = tracktion::createEmptyEdit(
            engine, temporary.getChildFile("preview.tracktionedit"));
        edit->ensureNumberOfAudioTracks(8);
        edit->clickTrackEnabled = false;
        edit->setCountInMode(tracktion::Edit::CountIn::none);
        const juce::String names[] = {"Glass keys", "Low end", "Percussion",
                                      "Texture"};
        const juce::Colour colours[] = {TapeStyle::blue, TapeStyle::green,
                                        TapeStyle::orange,
                                        juce::Colour(0xffa79ed8)};
        auto tracks = tracktion::getAudioTracks(*edit);
        for (int i = 0; i < 4; ++i) {
            auto file =
                temporary.getChildFile("phrase" + juce::String(i) + ".wav");
            if (!makeAudio(file, i))
                return 1;
            tracks[i]->setName(names[i]);
            tracks[i]->setColour(colours[i]);
            for (int take = 0; take < 2; ++take) {
                const double start = .4 + take * 2.4 + i * .15;
                tracks[i]->insertWaveClip(
                    "take " + juce::String(take + 1), file,
                    {tracktion::TimeRange(
                         tracktion::TimePosition::fromSeconds(start),
                         tracktion::TimePosition::fromSeconds(start + 2.2)),
                     tracktion::TimeDuration::fromSeconds(take * 2.2)},
                    false);
            }
        }
        app_services::MidiCommandManager midi(engine);
        TracksView view(*edit, midi);
        view.setSize(800, 480);
        view.addToDesktop(0);
        view.setVisible(true);
        AudioClock clock(audio);
        auto &model = view.getViewModel();
        auto &transport = edit->getTransport();
        transport.setCurrentPosition(0);
        pump(600);
        result |= !save(view, output.getChildFile("tape-start.png"));
        transport.setCurrentPosition(10);
        pump(100);
        result |= !save(view, output.getChildFile("tape-scrolled.png"));
        transport.setCurrentPosition(3.2);
        pump(600); // waveform thumbnails load asynchronously
        result |= !save(view, output.getChildFile("tape-stopped.png"));

        transport.setLoopRange({tracktion::TimePosition::fromSeconds(2),
                                tracktion::TimePosition::fromSeconds(5)});
        transport.looping = true;
        pump(80);
        result |= !save(view, output.getChildFile("tape-loop.png"));
        transport.looping = false;

        view.cutButtonReleased();
        if (tracks[0]->getClips().size() != 1)
            result = 1;
        pump(100);
        result |= !save(view, output.getChildFile("tape-lifted.png"));
        view.encoder3Increased();
        view.pasteButtonReleased();
        if (tracks[0]->getClips().size() != 2)
            result = 1;
        pump(100);
        view.encoder4Increased();
        view.encoder4Increased();
        pump(100);
        result |= !save(view, output.getChildFile("tape-zoom.png"));

        view.encoder1ButtonReleased();
        pump(100);
        result |= !save(view, output.getChildFile("tape-focus.png"));
        view.encoder1ButtonReleased();
        for (int i = 0; i < 5; ++i) {
            view.encoder1Increased();
            pump(30);
        }
        if (model.listViewModel.itemListState.getSelectedItemIndex() != 5)
            result = 1;
        result |= !save(view, output.getChildFile("tape-page-two.png"));
        model.listViewModel.itemListState.setSelectedItemIndex(0);
        pump(100);
        midi.isControlDown = true;
        pump(80);
        result |= !save(view, output.getChildFile("tape-control.png"));
        midi.isControlDown = false;

        // Full screen playback plus two reel frames verify motion without a
        // mock UI.
        transport.setCurrentPosition(3.2);
        pump(80);
        view.playButtonReleased();
        pump(150);
        result |= !save(view, output.getChildFile("tape-playing.png"));
        juce::Image firstReelFrame, lastReelFrame;
        for (int frame = 0; frame < 12; ++frame) {
            pump(70);
            auto frameImage =
                view.getChildComponent(0)->createComponentSnapshot(
                    view.getChildComponent(0)->getLocalBounds());
            if (frame == 0)
                firstReelFrame = frameImage;
            lastReelFrame = frameImage;
            result |= !save(
                *view.getChildComponent(0),
                output.getChildFile(
                    "reel-" + juce::String(frame).paddedLeft('0', 2) + ".png"));
            result |= !save(view, output.getChildFile(
                                      "tape-animation-" +
                                      juce::String(frame).paddedLeft('0', 2) +
                                      ".png"));
        }
        bool reelsMoved = false;
        for (int y = 25; y < firstReelFrame.getHeight(); ++y)
            for (int x = 0; x < firstReelFrame.getWidth() / 2; ++x)
                reelsMoved = reelsMoved || firstReelFrame.getPixelAt(x, y) !=
                                               lastReelFrame.getPixelAt(x, y);
        if (!reelsMoved)
            result = 1;
        view.stopButtonReleased();
        pump(100);
        const auto still1 = view.getChildComponent(0)->createComponentSnapshot(
            view.getChildComponent(0)->getLocalBounds());
        pump(100);
        const auto still2 = view.getChildComponent(0)->createComponentSnapshot(
            view.getChildComponent(0)->getLocalBounds());
        for (int y = 0; y < still1.getHeight(); ++y)
            for (int x = 0; x < still1.getWidth(); ++x)
                if (still1.getPixelAt(x, y) != still2.getPixelAt(x, y))
                    result = 1;

        auto synth = edit->getPluginCache().createNewPlugin(
            tracktion::FourOscPlugin::xmlTypeName, {});
        tracks[0]->pluginList.insertPlugin(synth, 0, nullptr);
        model.listViewModel.itemListState.setSelectedItemIndex(0);
        pump(100);
        midi.isControlDown = true;
        view.recordButtonReleased();
        midi.isControlDown = false;
        pump(300);
        if (!transport.isRecording())
            result = 1;
        result |= !save(view, output.getChildFile("tape-recording.png"));
        view.stopButtonReleased();
        pump(100);

        view.setSize(640, 480);
        pump(100);
        result |= !save(view, output.getChildFile("tape-compact.png"));
        tracktion::Clipboard::getInstance()->clear();
        std::cout << (result == 0 ? "Tape screen checks passed\n"
                                  : "Tape screen checks failed\n");
    }
    temporary.deleteRecursively();
    return result;
}
