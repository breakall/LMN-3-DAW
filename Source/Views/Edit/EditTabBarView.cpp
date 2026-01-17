#include "EditTabBarView.h"
#include "AvailableSequencersListView.h"
#include "FourOscView.h"
#include "MixerView.h"
#include "PluginView.h"
#include "SettingsListView.h"
#include "TempoSettingsView.h"
#include "TrackModifiersListView.h"
#include "TrackPluginsListView.h"
#include "TracksView.h"
#include <cmath>
#include <memory>
#include <thread>
EditTabBarView::EditTabBarView(tracktion::Edit &e,
                               app_services::MidiCommandManager &mcm)
    : TabbedComponent(juce::TabbedButtonBar::Orientation::TabsAtTop), edit(e),
      midiCommandManager(mcm), viewModel(edit) {
    // Note: Some tabs are on a per-track basis and are added in
    // selectedIndexChanged, not here this is possible since this view is a
    // listener of the tracks item list state
    addTab(tracksTabName, juce::Colours::transparentBlack,
           new TracksView(edit, midiCommandManager), true);
    addTab(tempoSettingsTabName, juce::Colours::transparentBlack,
           new TempoSettingsView(edit, midiCommandManager), true);
    addTab(mixerTabName, juce::Colours::transparentBlack,
           new MixerView(edit, midiCommandManager), true);
    addTab(settingsTabName, juce::Colours::transparentBlack,
           new app_navigation::StackNavigationController(new SettingsListView(
               edit, edit.engine.getDeviceManager().deviceManager,
               midiCommandManager)),
           true);

    juce::StringArray tabNames = getTabNames();
    int tracksIndex = tabNames.indexOf(tracksTabName);
    if (auto tracksView =
            dynamic_cast<TracksView *>(getTabContentComponent(tracksIndex))) {
        // this is so we can be notified when selected track changes
        tracksView->getViewModel().listViewModel.itemListState.addListener(
            this);
    }

    // hide tab bar
    setTabBarDepth(0);

    addChildComponent(octaveDisplayComponent);
    octaveDisplayComponent.setAlwaysOnTop(true);

    addChildComponent(messageBox);
    messageBox.setAlwaysOnTop(true);

    midiCommandManager.addListener(this);
    viewModel.addListener(this);

    // Set tracks as initial view
    setCurrentTabIndex(tracksIndex);
}

EditTabBarView::~EditTabBarView() {
    midiCommandManager.removeListener(this);
    viewModel.removeListener(this);
    juce::StringArray tabNames = getTabNames();
    int tracksIndex = tabNames.indexOf(tracksTabName);

    if (auto tracksView =
            dynamic_cast<TracksView *>(getTabContentComponent(tracksIndex)))
        tracksView->getViewModel().listViewModel.itemListState.removeListener(
            this);
}

void EditTabBarView::paint(juce::Graphics &g) {
    g.fillAll(
        getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void EditTabBarView::resized() {
    juce::TabbedComponent::resized();

    int octaveDisplayWidth = getWidth() / 6;
    int octaveDisplayHeight = getHeight() / 8;
    octaveDisplayComponent.setBounds((getWidth() - octaveDisplayWidth) / 2,
                                     (getHeight() - octaveDisplayHeight) / 2,
                                     octaveDisplayWidth, octaveDisplayHeight);

    auto font = messageBox.getFont();
    int width = font.getStringWidth(messageBox.getMessage());
    int messageBoxWidth = width + 50;
    int messageBoxHeight = getHeight() / 6;

    messageBox.setBounds((getWidth() - messageBoxWidth) / 2,
                         (getHeight() - messageBoxHeight) / 2, messageBoxWidth,
                         messageBoxHeight);
}

void EditTabBarView::tracksButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(tracksTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            // Clear the history so that only changes that happen on the tracks
            // tab can be undone ie dont let plugins that were added in the
            // plugins tab get removed or whatever
            edit.getUndoManager().clearUndoHistory();
            midiCommandManager.setFocusedComponent(
                getCurrentContentComponent());
        }
    }
}

void EditTabBarView::tempoSettingsButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(tempoSettingsTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            midiCommandManager.setFocusedComponent(
                getCurrentContentComponent());
        }
    }
}

/* void EditTabBarView::saveButtonReleased() {
    if (isShowing()) {
        juce::Logger::writeToLog("Saving edit ...");
        tracktion::EditFileOperations fileOperations(edit);
        fileOperations.save(true, true, false);
        juce::Logger::writeToLog("Save complete!");

        messageBox.setMessage("Save Complete!");
        // must call resized so message box width is updated to fit text
        resized();
        messageBox.setVisible(true);
        startTimer(1000);
    }
}*/

void EditTabBarView::saveButtonReleased() {
    const auto track_name = ConfigurationHelpers::getSavedTrackName();

    // editFile.create();
    tracktion::EditFileOperations fileOperations(edit);
    auto userAppDataDirectory = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory);

    juce::File savedDirectory =
        userAppDataDirectory.getChildFile(JUCE_APPLICATION_NAME_STRING)
            .getChildFile("saved");

    if (!savedDirectory.exists()) {
        if (!savedDirectory.createDirectory()) {
            juce::Logger::writeToLog("Error creating folder: " +
                                     savedDirectory.getFullPathName());
            return;
        }
    }

    if (track_name.exists()) {
        fileOperations.saveAs(track_name, true);
    }
    auto saveFile = savedDirectory.getChildFile(track_name.getFileName());

    if (track_name.copyFileTo(saveFile)) {
        juce::Logger::writeToLog("Track copied to: " +
                                 saveFile.getFullPathName());
    } else {
        juce::Logger::writeToLog("Error copying to: " +
                                 saveFile.getFullPathName());
    }

    juce::Logger::writeToLog("Complete! (" +
                             track_name.getFileNameWithoutExtension() + ")");
    messageBox.setMessage("Complete! (" +
                          track_name.getFileNameWithoutExtension() + ")");
    resized();
    messageBox.setVisible(true);
    startTimer(1000);
}

void EditTabBarView::renderButtonReleased() {
    if (isShowing()) {
        if (edit.getTransport().isPlaying()) {
            captureLastBars();
        } else {
            juce::Logger::writeToLog("Rendering edit ...");
            auto userAppDataDirectory = juce::File::getSpecialLocation(
                juce::File::userApplicationDataDirectory);

            auto renderFileName =
                std::to_string(juce::Time::currentTimeMillis());

            auto renderFile =
                userAppDataDirectory.getChildFile(applicationName)
                    .getChildFile("renders")
                    .getNonexistentChildFile(renderFileName, ".wav");

            auto timeRange = tracktion::TimeRange(
                tracktion::TimePosition::fromSeconds(0.0), edit.getLength());
            juce::BigInteger tracksToDo{0};
            for (auto i = 0; i < tracktion::getAllTracks(edit).size(); i++)
                tracksToDo.setBit(i);

            tracktion::Renderer::renderToFile(
                "Render", renderFile, edit, timeRange, tracksToDo, true, true,
                {}, true);
            juce::Logger::writeToLog("Render complete!");
            messageBox.setMessage("Render Complete!");
            // must call resized so message box width is updated to fit text
            resized();
            messageBox.setVisible(true);
            startTimer(1000);
        }
    }
}

void EditTabBarView::plusButtonPressed() {
    if (!midiCommandManager.isControlDown)
        return;

    if (captureBars < 8)
        captureBars *= 2;

    messageBox.setMessage("Capture bars: " + juce::String(captureBars));
    resized();
    messageBox.setVisible(true);
    startTimer(1000);
}

void EditTabBarView::minusButtonPressed() {
    if (!midiCommandManager.isControlDown)
        return;

    if (captureBars > 1)
        captureBars /= 2;

    messageBox.setMessage("Capture bars: " + juce::String(captureBars));
    resized();
    messageBox.setVisible(true);
    startTimer(1000);
}

void EditTabBarView::mixerButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(mixerTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            midiCommandManager.setFocusedComponent(
                getCurrentContentComponent());
        }
    }
}

void EditTabBarView::settingsButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(settingsTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent())) {
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());
            }
        } else {
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent())) {
                navigationController->popToRoot();
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());
            }
        }
    }
}

void EditTabBarView::captureLastBars() {
    auto *captureService = app_services::CaptureLastService::getInstance();
    if (captureService == nullptr || !captureService->isReady()) {
        messageBox.setMessage("Capture not ready");
        resized();
        messageBox.setVisible(true);
        startTimer(1000);
        return;
    }

    if (captureInProgress.load()) {
        messageBox.setMessage("Capture busy");
        resized();
        messageBox.setVisible(true);
        startTimer(1000);
        return;
    }

    const auto position = edit.getTransport().getPosition();
    const auto &timeSig = edit.tempoSequence.getTimeSigAt(position);
    const int denominator = juce::jmax(1, timeSig.denominator.get());
    const double beatsPerBar =
        timeSig.numerator.get() * (4.0 / static_cast<double>(denominator));
    auto endBeats = edit.tempoSequence.toBeats(position).inBeats();
    auto barEndBeats = std::floor(endBeats / beatsPerBar) * beatsPerBar;
    auto barEnd = edit.tempoSequence.toTime(
        tracktion::BeatPosition::fromBeats(barEndBeats));

    double startBeats = barEndBeats - (captureBars * beatsPerBar);
    if (startBeats < 0.0)
        startBeats = 0.0;

    auto barStart = edit.tempoSequence.toTime(
        tracktion::BeatPosition::fromBeats(startBeats));

    if (barStart >= barEnd) {
        messageBox.setMessage("Capture range too short");
        resized();
        messageBox.setVisible(true);
        startTimer(1000);
        return;
    }

    auto captureDir = edit.engine.getTemporaryFileManager()
                          .getTempDirectory()
                          .getChildFile("captures");
    captureDir.createDirectory();
    auto captureFile =
        captureDir.getNonexistentChildFile("capture", ".wav");

    tracktion::TimeRange range(barStart, barEnd);
    auto captureBuffer = std::make_shared<juce::AudioBuffer<float>>();
    if (!captureService->captureToBuffer(range, *captureBuffer)) {
        messageBox.setMessage("Capture failed");
        resized();
        messageBox.setVisible(true);
        startTimer(1000);
        return;
    }

    captureInProgress.store(true);
    messageBox.setMessage("Capturing...");
    resized();
    messageBox.setVisible(true);

    juce::Component::SafePointer<EditTabBarView> safeThis(this);
    auto captureFileCopy = captureFile;
    auto rangeCopy = range;
    std::thread([safeThis, captureBuffer, captureFileCopy, rangeCopy,
                 captureService]() {
        const bool ok = captureService != nullptr &&
                        captureService->writeBufferToFile(*captureBuffer,
                                                          captureFileCopy);
        juce::MessageManager::callAsync(
            [safeThis, ok, captureFileCopy, rangeCopy]() {
                if (safeThis == nullptr)
                    return;
                safeThis->handleCaptureWriteComplete(ok, captureFileCopy,
                                                     rangeCopy);
            });
    }).detach();
}

void EditTabBarView::handleCaptureWriteComplete(
    bool ok, const juce::File &captureFile,
    const tracktion::TimeRange &range) {
    captureInProgress.store(false);

    if (!ok) {
        messageBox.setMessage("Capture failed");
        resized();
        messageBox.setVisible(true);
        startTimer(1000);
        return;
    }

    auto audioTracks = tracktion::getAudioTracks(edit);
    if (audioTracks.size() == 0) {
        edit.ensureNumberOfAudioTracks(1);
        audioTracks = tracktion::getAudioTracks(edit);
    }

    if (auto *track = audioTracks.getFirst()) {
        tracktion::ClipPosition clipPosition{range};
        auto clip =
            track->insertWaveClip("capture", captureFile, clipPosition, false);
        if (clip != nullptr)
            messageBox.setMessage("Capture complete");
        else
            messageBox.setMessage("Capture clip failed");
    } else {
        messageBox.setMessage("No audio track");
    }

    resized();
    messageBox.setVisible(true);
    startTimer(1000);
}

void EditTabBarView::pluginsButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(pluginsTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent())) {
                if (auto pluginView = dynamic_cast<PluginView *>(
                        navigationController->getTopComponent())) {
                    // Four osc has a tab bar component, have to treat it
                    // special
                    if (auto fourOscView = dynamic_cast<FourOscView *>(
                            pluginView->getPlugin()
                                ->windowState->pluginWindow.get()))
                        midiCommandManager.setFocusedComponent(
                            fourOscView->getCurrentContentComponent());
                    else
                        midiCommandManager.setFocusedComponent(
                            pluginView->getPlugin()
                                ->windowState->pluginWindow.get());
                } else {
                    midiCommandManager.setFocusedComponent(
                        navigationController->getTopComponent());
                }
            }

        } else {
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent())) {
                navigationController->popToRoot();
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());
            }
        }
    }
}

void EditTabBarView::modifiersButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(modifiersTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent()))
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());

        } else {
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent())) {
                navigationController->popToRoot();
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());
            }
        }
    }
}

void EditTabBarView::sequencersButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(sequencersTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent()))
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());

        } else {
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent())) {
                navigationController->popToRoot();
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());
            }
        }
    }
}

void EditTabBarView::selectedIndexChanged(int /*newIndex*/) {
    resetTrackRelatedTabs();
}

void EditTabBarView::resetModifiersTab() {
    juce::StringArray tabNames = getTabNames();
    int tracksIndex = tabNames.indexOf(tracksTabName);
    if (auto tracksView =
            dynamic_cast<TracksView *>(getTabContentComponent(tracksIndex))) {
        if (auto track = dynamic_cast<tracktion::AudioTrack *>(
                tracksView->getViewModel().listViewModel.getSelectedItem())) {
            tabNames = getTabNames();
            int modifiersIndex = tabNames.indexOf(modifiersTabName);
            removeTab(modifiersIndex);

            addTab(modifiersTabName, juce::Colours::transparentBlack,
                   new app_navigation::StackNavigationController(
                       new TrackModifiersListView(track, midiCommandManager)),
                   true);
        }
    }
}

void EditTabBarView::octaveChanged(int newOctave) {
    viewModel.setCurrentOctave(newOctave);
    octaveDisplayComponent.setOctave(newOctave);
    octaveDisplayComponent.setVisible(true);
    startTimer(1000);
}

void EditTabBarView::timerCallback() {
    octaveDisplayComponent.setVisible(false);
    messageBox.setVisible(false);
}

void EditTabBarView::currentTabChanged(int /*newCurrentTabIndex*/,
                                       const juce::String &newCurrentTabName) {
    // A bunch of stuff happens in the sequencer view constructor
    // that needs to happen everytime it comes on screen
    // easiest way is just to remove and add the sequencers tab anytime a tab is
    // changed A better way would be to do this only when the SEQUENCER tab is
    // navigated away from, but this works for now The best way would be to
    // detect when the sequencer tab is shown and run some init method or
    // something
    if (newCurrentTabName != sequencersTabName) {
        juce::StringArray tabNames = getTabNames();
        int tracksIndex = tabNames.indexOf(tracksTabName);
        if (auto tracksView = dynamic_cast<TracksView *>(
                getTabContentComponent(tracksIndex))) {
            if (auto track = dynamic_cast<tracktion::AudioTrack *>(
                    tracksView->getViewModel()
                        .listViewModel.getSelectedItem())) {
                tabNames = getTabNames();
                int sequencersIndex = tabNames.indexOf(sequencersTabName);
                removeTab(sequencersIndex);

                addTab(sequencersTabName, juce::Colours::transparentBlack,
                       new app_navigation::StackNavigationController(
                           new AvailableSequencersListView(track,
                                                           midiCommandManager)),
                       true);
            }
        }
    }
}

void EditTabBarView::trackDeleted() { resetTrackRelatedTabs(); }

void EditTabBarView::resetTrackRelatedTabs() {
    juce::StringArray tabNames = getTabNames();
    int tracksIndex = tabNames.indexOf(tracksTabName);
    if (auto tracksView =
            dynamic_cast<TracksView *>(getTabContentComponent(tracksIndex))) {
        if (auto track = dynamic_cast<tracktion::AudioTrack *>(
                tracksView->getViewModel().listViewModel.getSelectedItem())) {
            tabNames = getTabNames();
            int sequencersIndex = tabNames.indexOf(sequencersTabName);
            removeTab(sequencersIndex);

            tabNames = getTabNames();
            int modifiersIndex = tabNames.indexOf(modifiersTabName);
            removeTab(modifiersIndex);

            tabNames = getTabNames();
            int pluginsIndex = tabNames.indexOf(pluginsTabName);
            removeTab(pluginsIndex);

            addTab(pluginsTabName, juce::Colours::transparentBlack,
                   new app_navigation::StackNavigationController(
                       new TrackPluginsListView(track, midiCommandManager)),
                   true);
            addTab(modifiersTabName, juce::Colours::transparentBlack,
                   new app_navigation::StackNavigationController(
                       new TrackModifiersListView(track, midiCommandManager)),
                   true);
            addTab(
                sequencersTabName, juce::Colours::transparentBlack,
                new app_navigation::StackNavigationController(
                    new AvailableSequencersListView(track, midiCommandManager)),
                true);
        }
    }
}
