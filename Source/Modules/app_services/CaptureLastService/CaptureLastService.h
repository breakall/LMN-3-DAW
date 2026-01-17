#pragma once

#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include <tracktion_engine/tracktion_engine.h>

namespace app_services {

class CaptureLastService {
  public:
    static void setInstance(CaptureLastService *service);
    static CaptureLastService *getInstance();

    void configure(double sampleRate, int numChannels, double bpm,
                   int beatsPerBar);

    void setCaptureBars(int bars);
    int getCaptureBars() const;

    void pushOutputBlock(const float **channels, int numChannels,
                         int numSamples);

    bool captureToFile(const tracktion::TimeRange &range,
                       const juce::File &outputFile);

    bool isReady() const;

  private:
    void updateBufferSize();

    static CaptureLastService *instance;

    juce::AudioBuffer<float> ringBuffer;
    std::atomic<int> writePosition{0};
    int totalSamples = 0;
    int configuredChannels = 0;
    double currentSampleRate = 0.0;
    double currentBpm = 120.0;
    int currentBeatsPerBar = 4;
    int maxBars = 8;
    int captureBars = 4;
};

} // namespace app_services
