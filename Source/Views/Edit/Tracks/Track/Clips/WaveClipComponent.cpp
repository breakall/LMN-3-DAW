#include "WaveClipComponent.h"
#include "TapeStyle.h"

WaveClipComponent::WaveClipComponent(tracktion::WaveAudioClip::Ptr c,
                                     app_services::TimelineCamera &cam)
    : ClipComponent(c.get(), cam) {
    formats.registerBasicFormats();
    thumbnail.addChangeListener(this);
    thumbnail.setSource(new juce::FileInputSource(c->getAudioFile().getFile()));
}

WaveClipComponent::~WaveClipComponent() {
    thumbnail.removeChangeListener(this);
}

void WaveClipComponent::paint(juce::Graphics &g) {
    auto wave = dynamic_cast<tracktion::WaveAudioClip *>(clip.get());
    auto track = wave != nullptr ? wave->getTrack() : nullptr;
    // A lifted clip can be painted once before the deferred view update removes
    // it.
    if (track == nullptr)
        return;
    const auto position = clip->getPosition();
    const double playhead = clip->edit.getTransport().getCurrentPosition();
    const bool underHead = trackSelected &&
                           position.getStart().inSeconds() <= playhead &&
                           playhead < position.getEnd().inSeconds();
    auto colour = track->getColour().brighter(.25f);
    if (track->isMuted(false))
        colour = TapeStyle::muted;
    g.setColour(colour.withAlpha(underHead ? .25f : .12f));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(.5f), 3);
    g.setColour(underHead ? TapeStyle::ink : colour.withAlpha(.7f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f), 3, 1);
    g.setFont(TapeStyle::mono(9));
    g.drawText(clip->getName(), 6, 2, juce::jmax(0, getWidth() - 12), 12,
               juce::Justification::centredLeft, true);
    const double speed = wave->getSpeedRatio();
    const double start = position.getOffset().inSeconds() * speed;
    const double end = start + position.getLength().inSeconds() * speed;
    if (end > start) {
        g.setColour(colour);
        auto waveform = getLocalBounds().reduced(3);
        waveform.removeFromTop(12);
        thumbnail.drawChannels(g, waveform, start, end, 1.0f);
    }
}

void WaveClipComponent::changeListenerCallback(juce::ChangeBroadcaster *) {
    repaint();
}
