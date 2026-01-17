#include "CaptureLastService.h"
#include <cmath>
#include <juce_audio_formats/juce_audio_formats.h>

namespace app_services {

CaptureLastService *CaptureLastService::instance = nullptr;

void CaptureLastService::setInstance(CaptureLastService *service) {
    instance = service;
}

CaptureLastService *CaptureLastService::getInstance() { return instance; }

void CaptureLastService::configure(double sampleRate, int numChannels,
                                   double bpm, int beatsPerBar) {
    currentSampleRate = sampleRate;
    configuredChannels = numChannels;
    currentBpm = bpm;
    currentBeatsPerBar = beatsPerBar;
    updateBufferSize();
}

void CaptureLastService::setCaptureBars(int bars) {
    captureBars = bars;
}

int CaptureLastService::getCaptureBars() const { return captureBars; }

bool CaptureLastService::isReady() const { return totalSamples > 0; }

double CaptureLastService::getSampleRate() const { return currentSampleRate; }

int CaptureLastService::getNumChannels() const { return configuredChannels; }

void CaptureLastService::updateBufferSize() {
    if (currentSampleRate <= 0.0 || configuredChannels <= 0)
        return;

    const double secondsPerBar =
        (60.0 / currentBpm) * static_cast<double>(currentBeatsPerBar);
    const double maxSeconds = secondsPerBar * static_cast<double>(maxBars);
    totalSamples = juce::jmax(
        1, static_cast<int>(std::lround(maxSeconds * currentSampleRate)));

    ringBuffer.setSize(configuredChannels, totalSamples);
    ringBuffer.clear();
    writePosition.store(0);
}

void CaptureLastService::pushOutputBlock(const float **channels,
                                        int numChannels, int numSamples) {
    if (totalSamples <= 0 || numChannels <= 0 || numSamples <= 0)
        return;

    const int channelsToCopy = juce::jmin(numChannels, configuredChannels);
    if (channelsToCopy <= 0)
        return;

    if (numSamples >= totalSamples) {
        const int offset = numSamples - totalSamples;
        for (int ch = 0; ch < channelsToCopy; ++ch) {
            if (channels[ch] == nullptr)
                continue;
            ringBuffer.copyFrom(ch, 0, channels[ch] + offset, totalSamples);
        }
        writePosition.store(0);
        return;
    }

    int writePos = writePosition.load();
    const int firstPart = juce::jmin(totalSamples - writePos, numSamples);
    const int secondPart = numSamples - firstPart;

    for (int ch = 0; ch < channelsToCopy; ++ch) {
        if (channels[ch] == nullptr)
            continue;
        ringBuffer.copyFrom(ch, writePos, channels[ch], firstPart);
        if (secondPart > 0)
            ringBuffer.copyFrom(ch, 0, channels[ch] + firstPart, secondPart);
    }

    writePos += numSamples;
    if (writePos >= totalSamples)
        writePos -= totalSamples;

    writePosition.store(writePos);
}

bool CaptureLastService::captureToBuffer(const tracktion::TimeRange &range,
                                        juce::AudioBuffer<float> &buffer) {
    if (!isReady())
        return false;

    const auto rangeSeconds = range.getLength().inSeconds();
    if (rangeSeconds <= 0.0)
        return false;

    const int numSamples =
        static_cast<int>(std::lround(rangeSeconds * currentSampleRate));
    if (numSamples <= 0 || numSamples > totalSamples)
        return false;

    buffer.setSize(configuredChannels, numSamples);

    int startSample = writePosition.load() - numSamples;
    if (startSample < 0)
        startSample += totalSamples;

    const int firstPart = juce::jmin(totalSamples - startSample, numSamples);
    const int secondPart = numSamples - firstPart;

    for (int ch = 0; ch < configuredChannels; ++ch) {
        buffer.copyFrom(ch, 0, ringBuffer, ch, startSample, firstPart);
        if (secondPart > 0)
            buffer.copyFrom(ch, firstPart, ringBuffer, ch, 0, secondPart);
    }

    return true;
}

bool CaptureLastService::captureToFile(const tracktion::TimeRange &range,
                                      const juce::File &outputFile) {
    juce::AudioBuffer<float> captureBuffer;
    if (!captureToBuffer(range, captureBuffer))
        return false;

    return writeBufferToFile(captureBuffer, outputFile);
}

bool CaptureLastService::writeBufferToFile(
    const juce::AudioBuffer<float> &buffer,
    const juce::File &outputFile) const {
    if (currentSampleRate <= 0.0 || buffer.getNumSamples() <= 0)
        return false;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outputStream(
        outputFile.createOutputStream());
    if (!outputStream)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(outputStream.get(), currentSampleRate,
                                  buffer.getNumChannels(), 24, {}, 0));
    if (!writer)
        return false;

    outputStream.release();
    return writer->writeFromAudioSampleBuffer(buffer, 0,
                                              buffer.getNumSamples());
}

} // namespace app_services
