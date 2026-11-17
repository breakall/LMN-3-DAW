#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {

class TapeRecordingTest : public ::testing::Test {
  protected:
    void SetUp() override {
        directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("lmn3-tape-test", "", false);
        ASSERT_TRUE(directory.createDirectory().wasOk());
        auto &audio = engine.getDeviceManager().getHostedAudioDeviceInterface();
        tracktion::HostedAudioDeviceInterface::Parameters parameters;
        parameters.inputChannels = 0;
        parameters.outputChannels = 2;
        parameters.sampleRate = 48000;
        parameters.blockSize = 256;
        audio.initialise(parameters);
        audio.prepareToPlay(48000, 256);
        edit = tracktion::createEmptyEdit(
            engine, directory.getChildFile("phrase.tracktionedit"));
        edit->ensureNumberOfAudioTracks(1);
        edit->clickTrackEnabled = false;
        edit->setCountInMode(tracktion::Edit::CountIn::none);
        model = std::make_unique<app_view_models::TracksListViewModel>(*edit,
                                                                       camera);
        flushSelection();
    }

    void TearDown() override {
        tracktion::Clipboard::getInstance()->clear();
        model.reset();
        edit.reset();
        engine.getDeviceManager().removeHostedAudioDeviceInterface();
        directory.deleteRecursively();
    }

    void flushSelection() {
        model->listViewModel.handleUpdateNowIfNeeded();
        model->listViewModel.itemListState.handleUpdateNowIfNeeded();
    }

    tracktion::AudioTrack *source() {
        return tracktion::getAudioTracks(*edit)[0];
    }

    tracktion::WaveAudioClip::Ptr insertPhrase() {
        juce::AudioBuffer<float> samples(2, 96000);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < samples.getNumSamples(); ++i)
                samples.setSample(c, i, i < 48000 ? .25f : -.5f);
        auto file = directory.getChildFile("source.wav");
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(
            new juce::FileOutputStream(file), 48000, 2, 16, {}, 0));
        if (!writer || !writer->writeFromAudioSampleBuffer(
                           samples, 0, samples.getNumSamples()))
            return {};
        writer.reset();
        auto clip = source()->insertWaveClip(
            "phrase", file,
            {tracktion::TimeRange(tracktion::TimePosition::fromSeconds(0),
                                  tracktion::TimePosition::fromSeconds(2)),
             tracktion::TimeDuration::fromSeconds(0)},
            false);
        edit->getUndoManager().clearUndoHistory();
        return clip;
    }

    void seek(double seconds) {
        edit->getTransport().setPosition(tracktion::TimePosition::fromSeconds(seconds));
    }

    tracktion::Engine engine{"Tape tests"};
    juce::File directory;
    std::unique_ptr<tracktion::Edit> edit;
    app_services::TimelineCamera camera{7};
    std::unique_ptr<app_view_models::TracksListViewModel> model;
};

TEST_F(TapeRecordingTest, SplitLiftDropPreservesAudioAndCanBeUndone) {
    auto original = insertPhrase();
    ASSERT_NE(original, nullptr);
    const auto file = original->getAudioFile().getFile();
    juce::MemoryBlock originalBytes;
    ASSERT_TRUE(file.loadFileAsData(originalBytes));
    seek(1);
    model->splitSelectedTracksClipAtPlayHead();
    ASSERT_EQ(source()->getClips().size(), 2);
    // Exactly at the seam, Cut must lift the right-hand section.
    model->cutSelectedTracksClipAtPlayHead();
    ASSERT_EQ(source()->getClips().size(), 1);
    EXPECT_DOUBLE_EQ(
        source()->getClips()[0]->getPosition().getEnd().inSeconds(), 1);
    seek(3);
    model->pasteClipboardContentToTrackAtPlayhead();
    ASSERT_EQ(source()->getClips().size(), 2);
    auto moved =
        dynamic_cast<tracktion::WaveAudioClip *>(source()->getClips()[1]);
    ASSERT_NE(moved, nullptr);
    EXPECT_DOUBLE_EQ(moved->getPosition().getStart().inSeconds(), 3);
    EXPECT_DOUBLE_EQ(moved->getPosition().getEnd().inSeconds(), 4);
    EXPECT_DOUBLE_EQ(moved->getPosition().getOffset().inSeconds(), 1);
    EXPECT_EQ(moved->getAudioFile().getFile(), file);
    juce::MemoryBlock editedBytes;
    ASSERT_TRUE(file.loadFileAsData(editedBytes));
    EXPECT_EQ(editedBytes, originalBytes);
    EXPECT_DOUBLE_EQ(edit->getTransport().getPosition().inSeconds(), 4);

    model->undo(); // drop
    ASSERT_EQ(source()->getClips().size(), 1);
    model->undo(); // lift
    ASSERT_EQ(source()->getClips().size(), 2);
    model->undo(); // split
    ASSERT_EQ(source()->getClips().size(), 1);
    EXPECT_DOUBLE_EQ(
        source()->getClips()[0]->getPosition().getLength().inSeconds(), 2);
}

TEST_F(TapeRecordingTest, EditingInGapOrAtEndDoesNotCutAnotherTake) {
    ASSERT_NE(insertPhrase(), nullptr);
    seek(2);
    model->splitSelectedTracksClipAtPlayHead();
    model->cutSelectedTracksClipAtPlayHead();
    seek(3);
    model->cutSelectedTracksClipAtPlayHead();
    EXPECT_EQ(source()->getClips().size(), 1);
    EXPECT_TRUE(tracktion::Clipboard::getInstance()->isEmpty());
}

TEST_F(TapeRecordingTest, RearrangedAudioSurvivesSaveAndReload) {
    auto original = insertPhrase();
    ASSERT_NE(original, nullptr);
    const auto file = original->getAudioFile().getFile();
    seek(1);
    model->splitSelectedTracksClipAtPlayHead();
    model->cutSelectedTracksClipAtPlayHead();
    seek(4);
    model->pasteClipboardContentToTrackAtPlayhead();
    ASSERT_TRUE(tracktion::EditFileOperations(*edit).save(false, true, false));
    original = nullptr;
    model.reset();
    edit.reset();
    edit = tracktion::loadEditFromFile(
        engine, directory.getChildFile("phrase.tracktionedit"));
    ASSERT_NE(edit, nullptr);
    ASSERT_EQ(source()->getClips().size(), 2);
    auto moved =
        dynamic_cast<tracktion::WaveAudioClip *>(source()->getClips()[1]);
    ASSERT_NE(moved, nullptr);
    EXPECT_DOUBLE_EQ(moved->getPosition().getStart().inSeconds(), 4);
    EXPECT_DOUBLE_EQ(moved->getPosition().getOffset().inSeconds(), 1);
    EXPECT_EQ(moved->getAudioFile().getFile(), file);
}

TEST_F(TapeRecordingTest, RefusesTapeRecordingWithoutSynthOrDuringPlayback) {
    EXPECT_TRUE(model->startTapeRecording().failed());
    EXPECT_EQ(tracktion::getAudioTracks(*edit).size(), 1);
    edit->getTransport().play(false);
    EXPECT_TRUE(model->startTapeRecording().failed());
    edit->getTransport().stop(false, false);
    EXPECT_EQ(tracktion::getAudioTracks(*edit).size(), 1);
}

TEST_F(TapeRecordingTest, StopCancelsPendingTapeRecording) {
    auto synth = edit->getPluginCache().createNewPlugin(
        tracktion::FourOscPlugin::xmlTypeName, {});
    source()->pluginList.insertPlugin(synth, 0, nullptr);
    ASSERT_TRUE(model->startTapeRecording().wasOk());
    ASSERT_TRUE(model->isTapeRecording());
    model->stopRecordingOrPlaying();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    EXPECT_FALSE(model->isTapeRecording());
    EXPECT_FALSE(edit->getTransport().isRecording());
    EXPECT_EQ(tracktion::getAudioTracks(*edit).size(), 1);
    EXPECT_FALSE(source()->getWaveInputDevice().isEnabled());
}

TEST_F(TapeRecordingTest, RecordsSynthOutputToWaveAndRestoresMidiRecording) {
    auto synth = edit->getPluginCache().createNewPlugin(
        tracktion::FourOscPlugin::xmlTypeName, {});
    ASSERT_NE(synth, nullptr);
    source()->pluginList.insertPlugin(synth, 0, nullptr);
    source()->setName("Synth");
    auto &manager = engine.getDeviceManager();
    auto &audio = manager.getHostedAudioDeviceInterface();
    auto midiDevice = manager.getMidiInDevice(0);
    ASSERT_NE(midiDevice, nullptr);
    midiDevice->setEnabled(true);
    midiDevice->setMonitorMode(tracktion::InputDevice::MonitorMode::automatic);
    model->listViewModel.itemListState.setSelectedItemIndex(0);
    // Route the hosted MIDI input just as the hardware input is routed.
    edit->getTransport().ensureContextAllocated();
    auto midiInput = edit->getCurrentPlaybackContext()->getInputFor(midiDevice.get());
    ASSERT_NE(midiInput, nullptr);
    ASSERT_TRUE(midiInput->setTarget(source()->itemID, true, nullptr, 0));
    midiInput->setRecordingEnabled(source()->itemID, true);
    edit->restartPlayback();
    auto result = model->startTapeRecording();
    ASSERT_TRUE(result.wasOk()) << result.getErrorMessage();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    ASSERT_TRUE(edit->getTransport().isRecording());
    ASSERT_TRUE(model->isTapeRecording());
    ASSERT_EQ(tracktion::getAudioTracks(*edit).size(), 2);
    EXPECT_FALSE(midiInput->isRecordingEnabled(source()->itemID));
    EXPECT_EQ(midiDevice->getMonitorMode(), tracktion::InputDevice::MonitorMode::on);
    model->undo();
    model->deleteSelectedTrack();
    EXPECT_EQ(tracktion::getAudioTracks(*edit).size(), 2);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    for (int block = 0; block < 220; ++block) {
        buffer.clear();
        midi.clear();
        if (block == 10)
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100),
                          0);
        if (block == 160)
            midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        audio.processBlock(buffer, midi);
        juce::Thread::sleep(1);
        if (block % 8 == 0)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(1);
    }
    model->stopRecordingOrPlaying();
    EXPECT_TRUE(midiInput->isRecordingEnabled(source()->itemID));
    EXPECT_EQ(midiDevice->getMonitorMode(), tracktion::InputDevice::MonitorMode::automatic);
    flushSelection();
    EXPECT_FALSE(model->isTapeRecording());
    EXPECT_TRUE(
        source()->getClips().isEmpty()); // audio only, no doubled MIDI take
    EXPECT_FALSE(source()->getWaveInputDevice().isEnabled());
    EXPECT_FALSE(edit->getEditInputDevices().isInputDeviceAssigned(
        source()->getWaveInputDevice()));
    auto tape = tracktion::getAudioTracks(*edit)[1];
    ASSERT_EQ(tape->getClips().size(), 1);
    auto wave = dynamic_cast<tracktion::WaveAudioClip *>(tape->getClips()[0]);
    ASSERT_NE(wave, nullptr);
    EXPECT_EQ(model->listViewModel.getSelectedItem(), tape);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(
        formats.createReaderFor(wave->getAudioFile().getFile()));
    ASSERT_NE(reader, nullptr);
    EXPECT_EQ(reader->numChannels, 2u);
    ASSERT_GT(reader->lengthInSamples, 0);
    juce::AudioBuffer<float> recording(2, (int)reader->lengthInSamples);
    reader->read(&recording, 0, recording.getNumSamples(), 0, true, true);
    EXPECT_GT(recording.getMagnitude(0, recording.getNumSamples()), .001f);

    seek(wave->getPosition().getStart().inSeconds() +
         wave->getPosition().getLength().inSeconds() / 2);
    model->splitSelectedTracksClipAtPlayHead();
    EXPECT_EQ(tape->getClips().size(), 2);
}

} // namespace AppViewModelsTests
