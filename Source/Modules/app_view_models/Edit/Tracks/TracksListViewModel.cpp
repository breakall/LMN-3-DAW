#include "TracksListViewModel.h"

namespace app_view_models {

TracksListViewModel::TracksListViewModel(tracktion::Edit &e,
                                         app_services::TimelineCamera &cam)
    : edit(e),

      camera(cam), adapter(std::make_unique<TracksListAdapter>(edit)),
      state(edit.state.getOrCreateChildWithName(IDs::TRACKS_LIST_VIEW_STATE,
                                                nullptr)),
      listViewModel(edit.state, state, tracktion::IDs::TRACK, adapter.get()) {
    initialiseInputs();
    listViewModel.itemListState.addListener(this);
    edit.state.addListener(this);
    edit.getTransport().addChangeListener(this);
    edit.getTransport().addListener(this);
    edit.getTransport().state.addListener(this);

    edit.getUndoManager().clearUndoHistory();

    tracksViewType.referTo(state, IDs::tracksListViewType, nullptr,
                           static_cast<int>(TracksViewType::MULTI_TRACK));

    edit.getTransport().setCurrentPosition(0.0);
}

TracksListViewModel::~TracksListViewModel() {
    if (isTapeRecording()) {
        edit.getTransport().stop(false, false);
        finishTapeRecording();
    }
    listViewModel.removeListener(this);
    edit.state.removeListener(this);
    edit.getTransport().removeChangeListener(this);
    edit.getTransport().removeListener(this);
}

void TracksListViewModel::initialiseInputs() {
    // set initial midi devices
    auto &deviceManager = edit.engine.getDeviceManager();
    for (int i = 0; i < deviceManager.getNumMidiInDevices(); i++) {
        if (auto midiInputDevice = deviceManager.getMidiInDevice(i)) {
            midiInputDevice->setEndToEndEnabled(true);
            midiInputDevice->setEnabled(true);
        }
    }

    edit.getTransport().ensureContextAllocated();

    for (auto instance : edit.getAllInputDevices()) {
        if (instance->getInputDevice().isMidi() &&
            !instance->getInputDevice().isTrackDevice()) {
            if (auto selectedTrack = dynamic_cast<tracktion::AudioTrack *>(
                    listViewModel.getSelectedItem())) {
                instance->setTargetTrack(*selectedTrack, 0, true);
                instance->setRecordingEnabled(*selectedTrack, true);
            }
        }
    }

    edit.restartPlayback();
}

void TracksListViewModel::addTrack() {
    if (isTapeRecording())
        return;
    edit.ensureNumberOfAudioTracks(listViewModel.getAdapter()->size() + 1);

    // since we are manually settings the selected index, we need to update the
    // size of the list as well.
    listViewModel.itemListState.listSize =
        listViewModel.getAdapter()->size() + 1;
    listViewModel.itemListState.setSelectedItemIndex(
        listViewModel.getAdapter()->size() - 1);
}

void TracksListViewModel::deleteSelectedTrack() {
    if (edit.getTransport().isRecording())
        return;
    if (auto track = dynamic_cast<tracktion::AudioTrack *>(
            listViewModel.getSelectedItem())) {
        // dont allow user to delete only remaining track
        if (listViewModel.getAdapter()->size() > 1) {
            edit.deleteTrack(track);

            // after deleting the track, we need to initialize the selected
            // tracks input
            initialiseInputs();
        }
    }
}

tracktion::Clip *TracksListViewModel::getClipAtPlayhead() {
    if (auto track = dynamic_cast<tracktion::AudioTrack *>(
            listViewModel.getSelectedItem())) {
        const auto time = edit.getTransport().getPosition();
        // Half-open ranges select the right-hand section at a split boundary.
        for (auto clip : track->getClips()) {
            const auto position = clip->getPosition();
            if (position.getStart() <= time && time < position.getEnd())
                return clip;
        }
    }
    return nullptr;
}

void TracksListViewModel::splitSelectedTracksClipAtPlayHead() {
    if (edit.getTransport().isRecording())
        return;
    if (auto clip = getClipAtPlayhead()) {
        const auto time = edit.getTransport().getPosition();
        if (time > clip->getPosition().getStart()) {
            edit.getUndoManager().beginNewTransaction("Split tape");
            clip->getClipTrack()->splitClip(*clip, time);
        }
    }
}

void TracksListViewModel::cutSelectedTracksClipAtPlayHead() {
    if (edit.getTransport().isRecording())
        return;
    if (auto clip = getClipAtPlayhead()) {
        auto content = std::make_unique<tracktion::Clipboard::Clips>();
        content->addClip(0, clip->state);
        tracktion::Clipboard::getInstance()->setContent(std::move(content));
        edit.getUndoManager().beginNewTransaction("Lift tape");
        clip->removeFromParentTrack();
    }
}

void TracksListViewModel::mergeSelectedTracksClipsAtPlayhead() {
    if (edit.getTransport().isRecording())
        return;
    if (auto track = dynamic_cast<tracktion::AudioTrack *>(
            listViewModel.getSelectedItem())) {
        if (auto clip1 = dynamic_cast<tracktion::MidiClip *>(
                track->getNextTrackItemAt(tracktion::TimePosition::fromSeconds(
                    track->edit.getTransport().getCurrentPosition() - .1)))) {
            if (auto clip2 = dynamic_cast<tracktion::MidiClip *>(
                    track->getNextTrackItemAt(
                        tracktion::TimePosition::fromSeconds(
                            track->edit.getTransport().getCurrentPosition() +
                            .1)))) {
                if (clip1->itemID != clip2->itemID) {
                    juce::Array<tracktion::MidiClip *> clips;
                    clips.add(clip1);
                    clips.add(clip2);
                    tracktion::mergeMidiClips(clips);
                }
            }
        }
    }
}

void TracksListViewModel::pasteClipboardContentToTrackAtPlayhead() {
    if (edit.getTransport().isRecording())
        return;
    if (auto track = dynamic_cast<tracktion::AudioTrack *>(
            listViewModel.getSelectedItem())) {
        auto clipboard = tracktion::Clipboard::getInstance();
        if (!clipboard->isEmpty()) {
            if (clipboard->hasContentWithType<tracktion::Clipboard::Clips>()) {
                auto clipContent =
                    clipboard
                        ->getContentWithType<tracktion::Clipboard::Clips>();
                tracktion::EditInsertPoint insertPoint(edit);
                insertPoint.setNextInsertPoint(
                    edit.getTransport().getPosition(), track);

                tracktion::Clipboard::ContentType::EditPastingOptions options(
                    edit, insertPoint);

                if (clipContent->clips.empty())
                    return;
                edit.getUndoManager().beginNewTransaction("Drop tape");
                double start = std::numeric_limits<double>::max();
                for (auto &i : clipContent->clips) {
                    auto end = i.hasBeatTimes
                                   ? edit.tempoSequence.toTime(i.startBeats)
                                         .inSeconds()
                                   : (static_cast<double>(i.state.getProperty(
                                         tracktion::IDs::start)));

                    start = std::min(start, end);
                }

                options.startTime = tracktion::TimePosition::fromSeconds(
                    edit.getTransport().getCurrentPosition() - start);
                clipContent->pasteIntoEdit(options);
                edit.getTransport().setCurrentPosition(
                    edit.getTransport().getCurrentPosition() +
                    static_cast<double>(clipContent->clips[0].state.getProperty(
                        tracktion::IDs::length)));
            }
        }
    }
}

TracksListViewModel::TracksViewType TracksListViewModel::getTracksViewType() {
    return static_cast<TracksViewType>(tracksViewType.get());
}

void TracksListViewModel::setTracksViewType(TracksViewType type) {
    tracksViewType.setValue(static_cast<int>(type), nullptr);
}

void TracksListViewModel::startRecording() {
    // ensure a selected track exists
    if (auto selectedTrack = dynamic_cast<tracktion::AudioTrack *>(
            listViewModel.getSelectedItem())) {
        // only start recording if we currently arent recording
        // and the selected track is armed
        if (EngineHelpers::isTrackArmed(*selectedTrack)) {
            auto &transport = edit.getTransport();
            if (!transport.isRecording() && !transport.isPlaying()) {
                transport.record(false);
            }
        }
    }
}

juce::Result TracksListViewModel::startTapeRecording() {
    auto &transport = edit.getTransport();
    if (transport.isPlaying() || transport.isRecording() || isTapeRecording())
        return juce::Result::fail("Stop playback before recording to tape.");

    auto source =
        dynamic_cast<tracktion::AudioTrack *>(listViewModel.getSelectedItem());
    if (source == nullptr)
        return juce::Result::fail("Select a synth track first.");
    bool hasSynth = false;
    for (auto plugin : source->pluginList.getPlugins())
        hasSynth = hasSynth || plugin->isSynth();
    if (!hasSynth)
        return juce::Result::fail("Select a track containing a synth first.");

    auto &device = source->getWaveInputDevice();
    if (edit.getEditInputDevices().isInputDeviceAssigned(device))
        return juce::Result::fail(
            "This synth already has an audio recording route.");
    if (edit.engine.getDeviceManager().getSampleRate() <= 0)
        return juce::Result::fail("Choose an audio output in Settings first.");

    edit.getUndoManager().beginNewTransaction("Record synth to tape");
    tapeSource = source;
    tapeTrack = edit.insertNewAudioTrack(
        tracktion::TrackInsertPoint(*source, false), nullptr);
    if (tapeTrack == nullptr) {
        tapeSource = nullptr;
        return juce::Result::fail("Could not create the tape track.");
    }
    tapeTrack->setName(source->getName() + " Tape");
    tapeTrack->setColour(source->getColour());

    // Live MIDI still reaches the synth, but only its audio is recorded.
    for (auto input : edit.getAllInputDevices()) {
        if (input->getInputDevice().isMidi() &&
            !input->getInputDevice().isTrackDevice() &&
            input->isOnTargetTrack(*source) &&
            input->isRecordingEnabled(*source)) {
            tapeMidiInputs.add(input->getInputDevice().getName());
            input->setRecordingEnabled(*source, false);
        }
    }

    device.setEnabled(true);
    device.setEndToEnd(
        false); // Monitor the source once, not through both tracks.
    device.setRecordTriggerDb(-100.0f);
    device.setOutputFormat("WAV");
    device.setMergeMode(tracktion::WaveInputDevice::getMergeModes()[0]);
    edit.getEditInputDevices().getInstanceStateForInputDevice(device);
    transport.ensureContextAllocated();
    auto context = edit.getCurrentPlaybackContext();
    auto input = context != nullptr ? context->getInputFor(&device) : nullptr;
    if (input == nullptr) {
        auto failedTrack = tapeTrack;
        finishTapeRecording();
        edit.deleteTrack(failedTrack);
        return juce::Result::fail(
            "Could not connect the synth to the tape track.");
    }
    input->setTargetTrack(*tapeTrack, 0, true);
    input->setRecordingEnabled(*tapeTrack, true);
    // The new audio tap must be in the graph before the first recording block.
    context->reallocate();
    transport.record(false);
    if (!transport.isRecording() || !input->isRecording()) {
        auto failedTrack = tapeTrack;
        transport.stop(true, false);
        finishTapeRecording();
        edit.deleteTrack(failedTrack);
        return juce::Result::fail("Could not start audio recording.");
    }
    return juce::Result::ok();
}

void TracksListViewModel::finishTapeRecording() {
    if (!isTapeRecording())
        return;
    auto recordedTrack = tapeTrack;
    auto source = tapeSource;
    tapeTrack = nullptr;
    tapeSource = nullptr;
    auto &device = source->getWaveInputDevice();
    // Removing the route also removes its input instance. Never retain that
    // pointer.
    auto inputState =
        edit.getEditInputDevices().getInstanceStateForInputDevice(device);
    device.setEnabled(false);
    inputState.getParent().removeChild(inputState, nullptr);
    for (auto input : edit.getAllInputDevices())
        if (tapeMidiInputs.contains(input->getInputDevice().getName()) &&
            input->isOnTargetTrack(*source))
            input->setRecordingEnabled(*source, true);
    tapeMidiInputs.clear();

    if (!recordedTrack->getClips().isEmpty()) {
        auto tracks = tracktion::getAudioTracks(edit);
        listViewModel.itemListState.listSize = tracks.size();
        listViewModel.itemListState.setSelectedItemIndex(
            tracks.indexOf(recordedTrack.get()));
    }
    edit.restartPlayback();
}

void TracksListViewModel::startPlaying() {
    auto &transport = edit.getTransport();

    if (!transport.isPlaying()) {
        transport.play(false);
    }
}

void TracksListViewModel::stopRecordingOrPlaying() {
    auto &transport = edit.getTransport();
    if (transport.isPlaying() || transport.isRecording()) {
        transport.stop(false, false);
        finishTapeRecording();

        tracktion::EditFileOperations fileOperations(edit);
        fileOperations.save(true, true, false);

    } else {
        // if we try to stop while currently not playing
        // return transport to beginning
        transport.setCurrentPosition(0.0);
    }
}

void TracksListViewModel::nudgeTransportForward() {
    auto &transport = edit.getTransport();
    if (!transport.isPlaying() && !transport.isRecording()) {
        // use the camera's nudge amount to nudge both the transport and camera
        transport.setCurrentPosition(transport.getCurrentPosition() +
                                     camera.getNudgeAmount());
    }
}

void TracksListViewModel::nudgeTransportBackward() {
    auto &transport = edit.getTransport();
    if (!transport.isPlaying() && !transport.isRecording()) {
        if (transport.getCurrentPosition() > camera.getNudgeAmount() + .01)
            transport.setCurrentPosition(transport.getCurrentPosition() -
                                         camera.getNudgeAmount());
        else
            transport.setCurrentPosition(0);
    }
}

void TracksListViewModel::nudgeTransportForwardToNearestBeat() {
    double nearestBeatTime =
        tracktion::TimecodeSnapType::get1BeatSnapType()
            .roundTimeUp(edit.getTransport().getPosition(), edit.tempoSequence)
            .inSeconds();

    // Check if we are on a beat already
    if (nearestBeatTime == edit.getTransport().getCurrentPosition()) {
        double secondsPerBeat =
            1.0 / edit.tempoSequence.getBeatsPerSecondAt(
                      tracktion::TimePosition::fromSeconds(0.0));
        edit.getTransport().setCurrentPosition(
            edit.getTransport().getCurrentPosition() + secondsPerBeat);

    } else {
        edit.getTransport().setCurrentPosition(nearestBeatTime);
    }
}

void TracksListViewModel::nudgeTransportBackwardToNearestBeat() {
    double nearestBeatTime =
        tracktion::TimecodeSnapType::get1BeatSnapType()
            .roundTimeDown(edit.getTransport().getPosition(),
                           edit.tempoSequence)
            .inSeconds();

    // Check if we are on a beat already
    if (nearestBeatTime == edit.getTransport().getCurrentPosition()) {
        double secondsPerBeat =
            1.0 / edit.tempoSequence.getBeatsPerSecondAt(
                      tracktion::TimePosition::fromSeconds(0.0));
        edit.getTransport().setCurrentPosition(
            edit.getTransport().getCurrentPosition() - secondsPerBeat);

    } else {
        edit.getTransport().setCurrentPosition(nearestBeatTime);
    }
}

void TracksListViewModel::setLoopIn() {
    if (!edit.getTransport().looping)
        edit.getTransport().looping.setValue(true, nullptr);

    double currentOut = edit.getTransport().loopPoint2->inSeconds();
    edit.getTransport().setLoopIn(edit.getTransport().getPosition());

    if (edit.getTransport().loopPoint1->inSeconds() >= currentOut)
        edit.getTransport().setLoopOut(edit.getTransport().loopPoint1.get());
    else
        edit.getTransport().setLoopOut(
            tracktion::TimePosition::fromSeconds(currentOut));
}

void TracksListViewModel::setLoopOut() {
    if (!edit.getTransport().looping)
        edit.getTransport().looping.setValue(true, nullptr);

    edit.getTransport().setLoopOut(edit.getTransport().getPosition());
}

void TracksListViewModel::toggleLooping() {
    edit.getTransport().looping.setValue(!edit.getTransport().looping.get(),
                                         nullptr);
}

void TracksListViewModel::nudgeLoopInForward() {
    if (!edit.getTransport().looping)
        edit.getTransport().looping.setValue(true, nullptr);

    edit.getTransport().setPosition(edit.getTransport().loopPoint1);
    nudgeTransportForward();
    setLoopIn();
}
void TracksListViewModel::nudgeLoopInBackward() {
    if (!edit.getTransport().looping)
        edit.getTransport().looping.setValue(true, nullptr);

    edit.getTransport().setPosition(edit.getTransport().loopPoint1);
    nudgeTransportBackward();
    setLoopIn();
}

void TracksListViewModel::nudgeLoopOutForward() {
    if (!edit.getTransport().looping)
        edit.getTransport().looping.setValue(true, nullptr);

    edit.getTransport().setPosition(edit.getTransport().loopPoint2);
    nudgeTransportForward();
    setLoopOut();
}
void TracksListViewModel::nudgeLoopOutBackward() {
    if (!edit.getTransport().looping)
        edit.getTransport().looping.setValue(true, nullptr);

    edit.getTransport().setPosition(edit.getTransport().loopPoint2);
    nudgeTransportBackward();
    setLoopOut();
}

void TracksListViewModel::nudgeLoopInForwardToNearestBeat() {
    if (!edit.getTransport().looping)
        edit.getTransport().looping.setValue(true, nullptr);
    edit.getTransport().setPosition(edit.getTransport().loopPoint1);

    double nearestBeatTime =
        tracktion::TimecodeSnapType::get1BeatSnapType()
            .roundTimeUp(edit.getTransport().getPosition(), edit.tempoSequence)
            .inSeconds();

    // Check if we are on a beat already
    if (nearestBeatTime == edit.getTransport().getCurrentPosition()) {
        double secondsPerBeat =
            1.0 / edit.tempoSequence.getBeatsPerSecondAt(
                      tracktion::TimePosition::fromSeconds(0.0));
        edit.getTransport().setCurrentPosition(
            edit.getTransport().getCurrentPosition() + secondsPerBeat);

    } else {
        edit.getTransport().setCurrentPosition(nearestBeatTime);
    }

    setLoopIn();
}

void TracksListViewModel::nudgeLoopInBackwardToNearestBeat() {
    if (!edit.getTransport().looping)
        edit.getTransport().looping.setValue(true, nullptr);

    edit.getTransport().setPosition(edit.getTransport().loopPoint1);

    double nearestBeatTime =
        tracktion::TimecodeSnapType::get1BeatSnapType()
            .roundTimeDown(edit.getTransport().getPosition(),
                           edit.tempoSequence)
            .inSeconds();

    // Check if we are on a beat already
    if (nearestBeatTime == edit.getTransport().getCurrentPosition()) {
        double secondsPerBeat =
            1.0 / edit.tempoSequence.getBeatsPerSecondAt(
                      tracktion::TimePosition::fromSeconds(0.0));
        edit.getTransport().setCurrentPosition(
            edit.getTransport().getCurrentPosition() - secondsPerBeat);

    } else {
        edit.getTransport().setCurrentPosition(nearestBeatTime);
    }

    setLoopIn();
}

void TracksListViewModel::nudgeLoopOutForwardToNearestBeat() {
    if (!edit.getTransport().looping)
        edit.getTransport().looping.setValue(true, nullptr);
    edit.getTransport().setPosition(edit.getTransport().loopPoint2);

    double nearestBeatTime =
        tracktion::TimecodeSnapType::get1BeatSnapType()
            .roundTimeUp(edit.getTransport().getPosition(), edit.tempoSequence)
            .inSeconds();

    // Check if we are on a beat already
    if (nearestBeatTime == edit.getTransport().getCurrentPosition()) {
        double secondsPerBeat =
            1.0 / edit.tempoSequence.getBeatsPerSecondAt(
                      tracktion::TimePosition::fromSeconds(0.0));
        edit.getTransport().setCurrentPosition(
            edit.getTransport().getCurrentPosition() + secondsPerBeat);

    } else {
        edit.getTransport().setCurrentPosition(nearestBeatTime);
    }

    setLoopOut();
}

void TracksListViewModel::nudgeLoopOutBackwardToNearestBeat() {
    if (!edit.getTransport().looping)
        edit.getTransport().looping.setValue(true, nullptr);

    edit.getTransport().setPosition(edit.getTransport().loopPoint2);

    double nearestBeatTime =
        tracktion::TimecodeSnapType::get1BeatSnapType()
            .roundTimeDown(edit.getTransport().getPosition(),
                           edit.tempoSequence)
            .inSeconds();

    // Check if we are on a beat already
    if (nearestBeatTime == edit.getTransport().getCurrentPosition()) {
        double secondsPerBeat =
            1.0 / edit.tempoSequence.getBeatsPerSecondAt(
                      tracktion::TimePosition::fromSeconds(0.0));
        edit.getTransport().setCurrentPosition(
            edit.getTransport().getCurrentPosition() - secondsPerBeat);

    } else {
        edit.getTransport().setCurrentPosition(nearestBeatTime);
    }

    setLoopOut();
}

bool TracksListViewModel::getSelectedTrackSoloState() {
    if (auto selectedTrack = dynamic_cast<tracktion::AudioTrack *>(
            listViewModel.getSelectedItem()))
        return selectedTrack->isSolo(false);
    else
        return false;
}

bool TracksListViewModel::getSelectedTrackMuteState() {
    if (auto selectedTrack = dynamic_cast<tracktion::AudioTrack *>(
            listViewModel.getSelectedItem()))
        return selectedTrack->isMuted(false);
    else
        return false;
}

void TracksListViewModel::toggleSolo() {
    if (auto selectedTrack = dynamic_cast<tracktion::AudioTrack *>(
            listViewModel.getSelectedItem()))
        selectedTrack->setSolo(!selectedTrack->isSolo(false));
}

void TracksListViewModel::toggleMute() {
    if (auto selectedTrack = dynamic_cast<tracktion::AudioTrack *>(
            listViewModel.getSelectedItem()))
        selectedTrack->setMute(!selectedTrack->isMuted(false));
}

void TracksListViewModel::setSelectedTrackColour(juce::Colour colour) {
    if (auto selectedTrack = dynamic_cast<tracktion::AudioTrack *>(
            listViewModel.getSelectedItem()))
        selectedTrack->setColour(colour);
}

juce::Colour TracksListViewModel::getSelectedTrackColour() {
    if (auto selectedTrack = dynamic_cast<tracktion::AudioTrack *>(
            listViewModel.getSelectedItem()))
        return selectedTrack->getColour();

    return juce::Colour();
}

void TracksListViewModel::setVideoPosition(tracktion::TimePosition timePosition,
                                           bool forceJump) {
    auto time = timePosition.inSeconds();
    if (time - camera.getCenter() > camera.getCenterOffsetLimit())
        camera.setCenter(time - camera.getCenterOffsetLimit());

    if (camera.getCenter() - time > camera.getCenterOffsetLimit())
        if (time > camera.getScope() / 2.0 - camera.getCenterOffsetLimit())
            camera.setCenter(camera.getCenterOffsetLimit() + time);

    if (time == 0.0)
        camera.setCenter(camera.getScope() / 2.0);
}

void TracksListViewModel::undo() {
    if (edit.getTransport().isRecording())
        return;
    if (edit.getUndoManager().canUndo())
        edit.getUndoManager().undo();
}

void TracksListViewModel::handleAsyncUpdate() {
    if (compareAndReset(shouldUpdateTracksViewType))
        listeners.call([this](Listener &l) {
            l.tracksViewTypeChanged(getTracksViewType());
        });

    if (compareAndReset(shouldUpdateLooping))
        listeners.call([this](Listener &l) {
            l.loopingChanged(edit.getTransport().looping.get());
        });

    if (compareAndReset(shouldUpdateSolo))
        if (auto selectedTrack = dynamic_cast<tracktion::AudioTrack *>(
                listViewModel.getSelectedItem()))
            listeners.call([selectedTrack](Listener &l) {
                l.soloStateChanged(selectedTrack->isSolo(false));
            });

    if (compareAndReset(shouldUpdateMute))
        if (auto selectedTrack = dynamic_cast<tracktion::AudioTrack *>(
                listViewModel.getSelectedItem()))
            listeners.call([selectedTrack](Listener &l) {
                l.muteStateChanged(selectedTrack->isMuted(false));
            });
}

void TracksListViewModel::selectedIndexChanged(int newIndex) {
    if (isTapeRecording())
        return;
    for (auto instance : edit.getAllInputDevices()) {
        if (instance->getInputDevice().isMidi() &&
            !instance->getInputDevice().isTrackDevice()) {
            if (auto selectedTrack = dynamic_cast<tracktion::AudioTrack *>(
                    listViewModel.getSelectedItem())) {
                instance->setTargetTrack(*selectedTrack, 0, true);
                instance->setRecordingEnabled(*selectedTrack, true);
            }
        }
    }

    edit.restartPlayback();
}

void TracksListViewModel::changeListenerCallback(juce::ChangeBroadcaster *) {
    if (isTapeRecording() && !edit.getTransport().isRecording())
        finishTapeRecording();
    if (edit.getTransport().isRecording()) {
        listeners.call([this](Listener &l) {
            l.isRecordingChanged(edit.getTransport().isRecording());
        });
        listeners.call([](Listener &l) { l.isPlayingChanged(false); });

    } else if (edit.getTransport().isPlaying()) {
        listeners.call([](Listener &l) { l.isRecordingChanged(false); });
        listeners.call([](Listener &l) { l.isPlayingChanged(true); });

    } else {
        listeners.call([](Listener &l) { l.isRecordingChanged(false); });
        listeners.call([](Listener &l) { l.isPlayingChanged(false); });
    }
}

void TracksListViewModel::valueTreePropertyChanged(
    juce::ValueTree &treeWhosePropertyHasChanged,
    const juce::Identifier &property) {
    if (treeWhosePropertyHasChanged == state)
        if (property == IDs::tracksListViewType)
            markAndUpdate(shouldUpdateTracksViewType);

    if (treeWhosePropertyHasChanged == edit.getTransport().state)
        if (property == tracktion::IDs::looping)
            markAndUpdate(shouldUpdateLooping);

    if (tracktion::TrackList::isTrack(treeWhosePropertyHasChanged))
        if (property == tracktion::IDs::solo)
            markAndUpdate(shouldUpdateSolo);

    if (tracktion::TrackList::isTrack(treeWhosePropertyHasChanged))
        if (property == tracktion::IDs::mute)
            markAndUpdate(shouldUpdateMute);
}

void TracksListViewModel::addListener(Listener *l) {
    listeners.add(l);
    l->isRecordingChanged(edit.getTransport().isRecording());
    l->isPlayingChanged(edit.getTransport().isPlaying());
    l->tracksViewTypeChanged(getTracksViewType());
    l->loopingChanged(edit.getTransport().looping.get());

    if (auto selectedTrack = dynamic_cast<tracktion::AudioTrack *>(
            listViewModel.getSelectedItem())) {
        l->soloStateChanged(selectedTrack->isSolo(false));
        l->muteStateChanged(selectedTrack->isMuted(false));
    }
}

void TracksListViewModel::removeListener(Listener *l) { listeners.remove(l); }

} // namespace app_view_models
