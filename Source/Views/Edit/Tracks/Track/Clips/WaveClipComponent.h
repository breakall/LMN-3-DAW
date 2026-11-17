#pragma once
#include "ClipComponent.h"

class WaveClipComponent : public ClipComponent, private juce::ChangeListener {
  public:
    WaveClipComponent(tracktion::WaveAudioClip::Ptr clip,
                      app_services::TimelineCamera &camera);
    ~WaveClipComponent() override;
    void paint(juce::Graphics &g) override;

  private:
    juce::AudioFormatManager formats;
    juce::AudioThumbnailCache cache{4};
    juce::AudioThumbnail thumbnail{128, formats, cache};
    void changeListenerCallback(juce::ChangeBroadcaster *) override;
};
