#pragma once
#include "TapeStyle.h"

class TapeDeckComponent : public juce::Component {
  public:
    void update(double position, bool playing, bool recording,
                const juce::String &trackName, const juce::String &clipboard);
    void paint(juce::Graphics &g) override;

  private:
    double position = 0, previousPosition = 0, previousTick = 0;
    float angle = 0;
    bool playing = false, recording = false;
    juce::String trackName, clipboard;
    void drawReel(juce::Graphics &g, juce::Point<float> centre, float radius,
                  float rotation, float tapeRadius);
};
