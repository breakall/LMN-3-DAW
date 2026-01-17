#include "CaptureTapPlugin.h"

namespace internal_plugins {

app_services::CaptureLastService *CaptureTapPlugin::captureService = nullptr;

CaptureTapPlugin::CaptureTapPlugin(tracktion::PluginCreationInfo info)
    : tracktion::Plugin(info) {}

CaptureTapPlugin::~CaptureTapPlugin() { notifyListenersOfDeletion(); }

const char *CaptureTapPlugin::xmlTypeName = "captureTap";

void CaptureTapPlugin::initialise(
    const tracktion::PluginInitialisationInfo & /*info*/) {}

void CaptureTapPlugin::deinitialise() {}

void CaptureTapPlugin::applyToBuffer(const tracktion::PluginRenderContext &fc) {
    if (captureService == nullptr || fc.destBuffer == nullptr)
        return;

    const int numSamples = fc.bufferNumSamples;
    if (numSamples <= 0)
        return;

    const int bufferChannels = fc.destBuffer->getNumChannels();
    const int channelsToCopy = juce::jmin(bufferChannels, 2);
    if (channelsToCopy <= 0)
        return;

    const float *channelData[2] = {nullptr, nullptr};
    for (int ch = 0; ch < channelsToCopy; ++ch)
        channelData[ch] = fc.destBuffer->getReadPointer(ch);

    captureService->pushOutputBlock(channelData, channelsToCopy, numSamples);
}

void CaptureTapPlugin::setCaptureService(
    app_services::CaptureLastService *service) {
    captureService = service;
}

} // namespace internal_plugins
