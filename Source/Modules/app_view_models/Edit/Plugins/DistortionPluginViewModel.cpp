namespace app_view_models {

DistortionPluginViewModel::DistortionPluginViewModel(
    internal_plugins::DistortionPlugin *p)
    : InternalPluginViewModel(p), distortionPlugin(p) {}

int DistortionPluginViewModel::getNumberOfParameters() { return 1; }

juce::String DistortionPluginViewModel::getParameterName(int index) {
    switch (index) {
    case 0:
        return "Gain";
        break;
    default:
        return "Parameter " + juce::String(index);
        break;
    }
}

double DistortionPluginViewModel::getParameterValue(int index) {
    switch (index) {
    case 0:
        return distortionPlugin->gain.get();
    default:
        return distortionPlugin->gain.get();
    }
}

void DistortionPluginViewModel::setParameterValue(int index, double value) {
    switch (index) {
    case 0:
        if (distortionPlugin->gainParam->getModifiers().size() == 0)
            distortionPlugin->gainParam->setParameter(
                (float)getParameterRange(index).clipValue(value),
                juce::dontSendNotification);
        break;
    default:
        break;
    }
}

juce::Range<double> DistortionPluginViewModel::getParameterRange(int index) {
    switch (index) {
    case 0:
        return juce::Range<double>(0.1, 20.0);
    default:
        return juce::Range<double>(0.1, 20.0);
    }
}

double DistortionPluginViewModel::getParameterInterval(int index) {
    switch (index) {
    case 0:
        return .1;
    default:
        return .1;
    }
}

} // namespace app_view_models
