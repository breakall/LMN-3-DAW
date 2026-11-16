#include "TrackView.h"
#include "MidiClipComponent.h"
#include "WaveClipComponent.h"

TrackView::TrackView(tracktion::AudioTrack::Ptr t,
                     app_services::TimelineCamera &cam)
    : track(t), camera(cam), viewModel(track, cam) {
    selectedTrackMarker.setAlwaysOnTop(true);

    viewModel.addListener(this);

    addChildComponent(selectedTrackMarker);

    startTimerHz(30);
}

TrackView::~TrackView() { viewModel.removeListener(this); }

void TrackView::paint(juce::Graphics &g) {
    g.fillAll(juce::Colour(0x00282828));
}

void TrackView::resized() {
    selectedTrackMarker.setBounds(getLocalBounds());

    for (auto clipComponent : clips) {
        auto &clip = clipComponent->getClip();
        auto pos = clip.getPosition();
        int clipStart = juce::roundToInt(
            camera.timeToX(pos.getStart().inSeconds(), getWidth()));
        int clipEnd = juce::roundToInt(
            camera.timeToX(pos.getEnd().inSeconds(), getWidth()));
        clipComponent->setBounds(clipStart, 0, clipEnd - clipStart,
                                 getHeight());
    }
}

void TrackView::setSelected(bool selected) {
    isSelected = selected;
    selectedTrackMarker.setVisible(false);
    for (auto component : clips)
        component->setTrackSelected(selected);
}

void TrackView::clipsChanged(const juce::Array<tracktion::Clip *> &clips) {
    buildClips();
}
void TrackView::clipPositionsChanged(
    const juce::Array<tracktion::Clip *> &clips) {
    resized();
    for (auto component : this->clips)
        component->repaint();
}

void TrackView::transportChanged() {
    buildRecordingClip();
    buildClips();
    // resized();
}

void TrackView::buildClips() {
    clips.clear();

    if (auto clipTrack = dynamic_cast<tracktion::ClipTrack *>(
            dynamic_cast<tracktion::Track *>(track.get()))) {
        for (auto clip : clipTrack->getClips()) {
            if (auto midiClip = dynamic_cast<tracktion::MidiClip *>(clip)) {
                clips.add(new MidiClipComponent(clip, camera));
                addAndMakeVisible(clips.getLast());
            } else if (auto waveClip =
                           dynamic_cast<tracktion::WaveAudioClip *>(clip)) {
                clips.add(new WaveClipComponent(waveClip, camera));
                addAndMakeVisible(clips.getLast());
            }
        }
    }

    for (auto component : clips)
        component->setTrackSelected(isSelected);
    resized();
}

void TrackView::buildRecordingClip() {
    bool needed = false;

    if (track->edit.getTransport().isRecording()) {
        for (auto in : track->edit.getAllInputDevices()) {
            if (in->isRecordingActive() &&
                in->getTargetTracks().contains(track.get())) {
                needed = true;
                break;
            }
        }
    }

    if (needed) {
        recordingClip = std::make_unique<RecordingClipComponent>(track, camera);
        addAndMakeVisible(*recordingClip);
    } else {
        recordingClip = nullptr;
    }
}

void TrackView::timerCallback() { resized(); }
