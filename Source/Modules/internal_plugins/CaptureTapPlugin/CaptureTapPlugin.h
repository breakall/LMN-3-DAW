#pragma once
#include <app_services/app_services.h>

namespace internal_plugins {

class CaptureTapPlugin : public tracktion::Plugin {
  public:
    CaptureTapPlugin(tracktion::PluginCreationInfo);
    ~CaptureTapPlugin() override;

    static const char *getPluginName() { return "Capture Tap"; }
    static const char *xmlTypeName;

    juce::String getName() const override { return "Capture Tap"; }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName(int) override { return "Capture"; }

    void initialise(const tracktion::PluginInitialisationInfo &) override;
    void deinitialise() override;
    void applyToBuffer(const tracktion::PluginRenderContext &) override;
    juce::String getSelectableDescription() override {
        return "Capture Tap Plugin";
    }

    static void setCaptureService(app_services::CaptureLastService *service);

  private:
    static app_services::CaptureLastService *captureService;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CaptureTapPlugin)
};

} // namespace internal_plugins
