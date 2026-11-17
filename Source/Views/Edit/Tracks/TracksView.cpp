#include "TracksView.h"
#include "TapeStyle.h"
#include <cmath>

TracksView::TracksView(tracktion::Edit &e,
                       app_services::MidiCommandManager &mcm)
    : edit(e), midiCommandManager(mcm), camera(7), viewModel(e, camera) {
    setOpaque(true);
    addAndMakeVisible(deck);
    midiCommandManager.addListener(this);
    midiCommandManager.setFocusedComponent(this);
    viewModel.addListener(this);
    viewModel.listViewModel.addListener(this);
    viewModel.listViewModel.itemListState.addListener(this);
    rebuildLanes();
    updateDeck();
    startTimerHz(30);
}

TracksView::~TracksView() {
    stopTimer();
    midiCommandManager.removeListener(this);
    viewModel.removeListener(this);
    viewModel.listViewModel.removeListener(this);
    viewModel.listViewModel.itemListState.removeListener(this);
}

void TracksView::resized() {
    if (getWidth() <= 0 || getHeight() <= 0)
        return;
    const int margin = 8;
    // Keep four lanes while following the menu row typography and spacing.
    const int headerHeight = juce::jlimit(80, 104, getHeight() / 5);
    // The selected name is already in the deck header; lanes need only an ID.
    labelWidth = 80;
    deck.setBounds(margin, 4, getWidth() - 2 * margin, headerHeight);
    controlsBounds = {margin, getHeight() - 108, getWidth() - 2 * margin, 100};
    const int laneTop = deck.getBottom() + 24;
    laneBounds = {margin, laneTop, getWidth() - 2 * margin,
                  juce::jmax(32, controlsBounds.getY() - 32 - laneTop)};
    rulerBounds = {margin + labelWidth, laneTop - 24,
                   laneBounds.getWidth() - labelWidth, 24};
    const int count = viewModel.getTracksViewType() ==
                              app_view_models::TracksListViewModel::
                                  TracksViewType::SINGLE_TRACK
                          ? 1
                          : 4;
    for (int i = 0; i < lanes.size(); ++i) {
        const int top = laneBounds.getY() + laneBounds.getHeight() * i / count;
        const int bottom =
            laneBounds.getY() + laneBounds.getHeight() * (i + 1) / count;
        lanes[i]->setBounds(rulerBounds.getX(), top + 3, rulerBounds.getWidth(),
                            bottom - top - 6);
    }
    repaint();
}

void TracksView::rebuildLanes() {
    auto tracks = tracktion::getAudioTracks(edit);
    const int selected =
        viewModel.listViewModel.itemListState.getSelectedItemIndex();
    const bool focused =
        viewModel.getTracksViewType() ==
        app_view_models::TracksListViewModel::TracksViewType::SINGLE_TRACK;
    firstVisibleTrack =
        focused ? juce::jmax(0, selected) : juce::jmax(0, selected / 4 * 4);
    juce::Array<tracktion::AudioTrack::Ptr> next;
    for (int i = firstVisibleTrack;
         i < juce::jmin(tracks.size(), firstVisibleTrack + (focused ? 1 : 4));
         ++i)
        next.add(tracks[i]);
    if (next != visibleTracks) {
        lanes.clear();
        visibleTracks = next;
        for (auto track : visibleTracks) {
            auto lane = lanes.add(new TrackView(track, camera));
            addAndMakeVisible(lane);
        }
    }
    for (int i = 0; i < lanes.size(); ++i)
        lanes[i]->setSelected(firstVisibleTrack + i == selected);
    resized();
}

void TracksView::paint(juce::Graphics &g) {
    g.fillAll(TapeStyle::background);
    const int selected =
        viewModel.listViewModel.itemListState.getSelectedItemIndex();
    const bool focused =
        viewModel.getTracksViewType() ==
        app_view_models::TracksListViewModel::TracksViewType::SINGLE_TRACK;
    const int count = focused ? 1 : 4;
    for (int i = 0; i < count; ++i) {
        const int y = laneBounds.getY() + laneBounds.getHeight() * i / count;
        const int bottom =
            laneBounds.getY() + laneBounds.getHeight() * (i + 1) / count;
        const bool active =
            i < visibleTracks.size() && firstVisibleTrack + i == selected;
        auto row = juce::Rectangle<int>(laneBounds.getX(), y + 2,
                                        laneBounds.getWidth(), bottom - y - 4);
        g.setColour(active ? juce::Colour(0xff1d2c33)
                           : TapeStyle::panel.withAlpha(.6f));
        g.fillRoundedRectangle(row.toFloat(), 5);
        if (active) {
            g.setColour(TapeStyle::blue);
            g.fillRoundedRectangle(static_cast<float>(row.getX()),
                                   static_cast<float>(row.getY() + 6), 3.0f,
                                   static_cast<float>(row.getHeight() - 12),
                                   1.5f);
        }
        g.setFont(TapeStyle::rowFont(52));
        g.setColour(active ? TapeStyle::blue : TapeStyle::muted);
        g.drawText(juce::String(firstVisibleTrack + i + 1).paddedLeft('0', 2),
                   row.getX() + 10, row.getY(), 44, row.getHeight(),
                   juce::Justification::centredLeft);
        if (i < visibleTracks.size()) {
            const auto track = visibleTracks[i];
            const auto badge = track->isMuted(false)  ? "M"
                               : track->isSolo(false) ? "S"
                                                      : "";
            if (juce::String(badge).isNotEmpty()) {
                g.setColour(TapeStyle::orange);
                g.setFont(TapeStyle::mono(24));
                g.drawText(badge, row.getX() + labelWidth - 24, row.getCentreY() - 14, 24, 28,
                           juce::Justification::centred);
            }
        }
    }
    drawRuler(g);
    drawControls(g);
    g.setFont(TapeStyle::mono(24));
    g.setColour(TapeStyle::muted);
    g.drawText(message, laneBounds.getX(), controlsBounds.getY() - 30,
               laneBounds.getWidth(), 28, juce::Justification::centredLeft,
               true);
}

void TracksView::drawRuler(juce::Graphics &g) {
    if (rulerBounds.isEmpty())
        return;
    g.setFont(TapeStyle::mono(24));
    g.setColour(TapeStyle::muted);
    g.drawText("s", laneBounds.getX() + 12, rulerBounds.getY(),
               labelWidth - 12, rulerBounds.getHeight(),
               juce::Justification::centredLeft);
    const double step = camera.getScope() <= 2    ? .5
                        : camera.getScope() <= 10 ? 1
                        : camera.getScope() <= 30 ? 5
                                                  : 10;
    const double left = camera.getCenter() - camera.getScope() / 2;
    for (double t = std::ceil(left / step) * step;
         t <= left + camera.getScope(); t += step) {
        if (t < 0)
            continue;
        const int x =
            rulerBounds.getX() +
            juce::roundToInt(camera.timeToX(t, rulerBounds.getWidth()));
        g.setColour(TapeStyle::line);
        g.drawVerticalLine(x, static_cast<float>(laneBounds.getY()),
                           static_cast<float>(laneBounds.getBottom()));
        g.setColour(TapeStyle::muted);
        g.drawText(
            juce::String(std::abs(t) < .000001 ? 0.0 : t, step < 1 ? 1 : 0),
            juce::jlimit(rulerBounds.getX(), rulerBounds.getRight() - 64,
                         x - 32), rulerBounds.getY(), 64, rulerBounds.getHeight() - 4,
            x - 32 < rulerBounds.getX() ? juce::Justification::centredLeft
            : x + 32 > rulerBounds.getRight() ? juce::Justification::centredRight
                                               : juce::Justification::centred);
    }
}

void TracksView::paintOverChildren(juce::Graphics &g) {
    if (rulerBounds.isEmpty())
        return;
    auto &transport = edit.getTransport();
    const int x = rulerBounds.getX() + juce::roundToInt(
        camera.timeToX(transport.getCurrentPosition(), rulerBounds.getWidth()));
    if (transport.looping) {
        const int left =
            rulerBounds.getX() +
            juce::roundToInt(camera.timeToX(transport.loopPoint1->inSeconds(),
                                            rulerBounds.getWidth()));
        const int right =
            rulerBounds.getX() +
            juce::roundToInt(camera.timeToX(transport.loopPoint2->inSeconds(),
                                            rulerBounds.getWidth()));
        const juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(rulerBounds);
        g.setColour(TapeStyle::green);
        g.drawLine(static_cast<float>(left),
                   static_cast<float>(rulerBounds.getBottom() - 2),
                   static_cast<float>(right),
                   static_cast<float>(rulerBounds.getBottom() - 2), 2);
        g.fillRect(left, rulerBounds.getBottom() - 8, 2, 8);
        g.fillRect(right, rulerBounds.getBottom() - 8, 2, 8);
    }
    g.setColour(transport.isRecording() ? TapeStyle::red : TapeStyle::ink);
    g.fillRect(x, laneBounds.getY(), 1, laneBounds.getHeight());
    juce::Path head;
    head.addTriangle(
        static_cast<float>(x - 5), static_cast<float>(laneBounds.getY() - 5),
        static_cast<float>(x + 6), static_cast<float>(laneBounds.getY() - 5),
        static_cast<float>(x), static_cast<float>(laneBounds.getY() + 1));
    g.fillPath(head);
}

void TracksView::drawControls(juce::Graphics &g) {
    const bool control = midiCommandManager.isControlDown;
    auto &transport = edit.getTransport();
    const int selected =
        viewModel.listViewModel.itemListState.getSelectedItemIndex();
    const auto total = tracktion::getAudioTracks(edit).size();
    const juce::Colour colours[] = {TapeStyle::blue, TapeStyle::green,
                                    TapeStyle::ink, TapeStyle::orange};
    const juce::String titles[] = {"Track", control ? "Loop in" : "Loop out",
                                   control ? "Beat" : "Position",
                                   control ? "Colour" : "Zoom"};
    const juce::String values[] = {
        juce::String(juce::jmax(0, selected + 1)).paddedLeft('0', 2) + "/" +
            juce::String(total).paddedLeft('0', 2),
        TapeStyle::time(control ? transport.loopPoint1->inSeconds()
                                : transport.loopPoint2->inSeconds()).dropLastCharacters(2),
        TapeStyle::time(transport.getCurrentPosition()).dropLastCharacters(2),
        control ? "Palette" : juce::String(camera.getScope(), 1) + " s"};
    for (int i = 0; i < 4; ++i) {
        const int x = controlsBounds.getX() + controlsBounds.getWidth() * i / 4;
        const int right =
            controlsBounds.getX() + controlsBounds.getWidth() * (i + 1) / 4;
        auto card =
            juce::Rectangle<int>(x + (i == 0 ? 0 : 4), controlsBounds.getY(),
                                 right - x - 4, controlsBounds.getHeight());
        g.setColour(TapeStyle::panel);
        g.fillRoundedRectangle(card.toFloat(), 5);
        g.setColour(colours[i]);
        // Two generous text rows replace three lines of miniature labels.
        g.setFont(TapeStyle::rowFont(getWidth() < 720 ? 34 : 40));
        g.drawText(titles[i], card.getX() + 10, card.getY() + 6,
                   card.getWidth() - 20, 38, juce::Justification::centredLeft);
        g.setFont(TapeStyle::rowFont(getWidth() < 720 ? 42 : 52));
        g.drawText(values[i], card.getX() + 10, card.getY() + 44,
                   card.getWidth() - 20, 52, juce::Justification::centredLeft);
    }
}

void TracksView::changeZoom(double factor) {
    camera.setScope(juce::jlimit(.5, 120.0, camera.getScope() * factor));
    camera.setNudgeAmount(camera.getScope() / 80);
    // Do not reserve screen space for negative tape time.
    camera.setCenter(juce::jmax(camera.getScope() / 2.0,
                              edit.getTransport().getCurrentPosition()));
    for (auto lane : lanes)
        lane->resized();
    repaint();
}

void TracksView::encoder1Increased() {
    if (viewModel.isTapeRecording())
        return;
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.listViewModel.itemListState.setSelectedItemIndex(
                viewModel.listViewModel.itemListState.getSelectedItemIndex() +
                1);
}

void TracksView::encoder1Decreased() {
    if (viewModel.isTapeRecording())
        return;
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.listViewModel.itemListState.setSelectedItemIndex(
                viewModel.listViewModel.itemListState.getSelectedItemIndex() -
                1);
}

void TracksView::encoder1ButtonReleased() {
    if (isShowing() && midiCommandManager.getFocusedComponent() == this)
        viewModel.setTracksViewType(
            viewModel.getTracksViewType() ==
                    app_view_models::TracksListViewModel::TracksViewType::
                        MULTI_TRACK
                ? app_view_models::TracksListViewModel::TracksViewType::
                      SINGLE_TRACK
                : app_view_models::TracksListViewModel::TracksViewType::
                      MULTI_TRACK);
}

void TracksView::encoder2Increased() {
    if (isShowing()) {
        if (midiCommandManager.getFocusedComponent() == this) {
            if (midiCommandManager.isControlDown)
                viewModel.nudgeLoopInForwardToNearestBeat();
            else
                viewModel.nudgeLoopOutForwardToNearestBeat();
        }
    }
}
void TracksView::encoder2Decreased() {
    if (isShowing()) {
        if (midiCommandManager.getFocusedComponent() == this) {
            if (midiCommandManager.isControlDown)
                viewModel.nudgeLoopInBackwardToNearestBeat();
            else
                viewModel.nudgeLoopOutBackwardToNearestBeat();
        }
    }
}

void TracksView::encoder3Increased() {
    if (isShowing()) {
        if (midiCommandManager.getFocusedComponent() == this) {
            if (midiCommandManager.isControlDown)
                viewModel.nudgeTransportForwardToNearestBeat();
            else
                viewModel.nudgeTransportForward();
        }
    }
}

void TracksView::encoder3Decreased() {
    if (isShowing()) {
        if (midiCommandManager.getFocusedComponent() == this) {
            if (midiCommandManager.isControlDown)
                viewModel.nudgeTransportBackwardToNearestBeat();
            else
                viewModel.nudgeTransportBackward();
        }
    }
}

void TracksView::encoder3ButtonReleased() {
    if (isShowing() && midiCommandManager.getFocusedComponent() == this)
        viewModel.toggleSolo();
}

void TracksView::encoder4Increased() {
    if (!isShowing() || midiCommandManager.getFocusedComponent() != this)
        return;
    if (!midiCommandManager.isControlDown) {
        changeZoom(0.8);
        return;
    }
    int colourIndex =
        appLookAndFeel.colours.indexOf(viewModel.getSelectedTrackColour());
    if (colourIndex == appLookAndFeel.colours.size() - 1)
        colourIndex = 0;
    else
        colourIndex++;

    viewModel.setSelectedTrackColour(appLookAndFeel.colours[colourIndex]);
}

void TracksView::encoder4Decreased() {
    if (!isShowing() || midiCommandManager.getFocusedComponent() != this)
        return;
    if (!midiCommandManager.isControlDown) {
        changeZoom(1.25);
        return;
    }
    int colourIndex =
        appLookAndFeel.colours.indexOf(viewModel.getSelectedTrackColour());
    if (colourIndex == 0)
        colourIndex = appLookAndFeel.colours.size() - 1;
    else
        colourIndex--;

    viewModel.setSelectedTrackColour(appLookAndFeel.colours[colourIndex]);
}

void TracksView::encoder4ButtonReleased() {
    if (isShowing() && midiCommandManager.getFocusedComponent() == this)
        viewModel.toggleMute();
}

void TracksView::cutButtonReleased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.cutSelectedTracksClipAtPlayHead();
}

void TracksView::pasteButtonReleased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.pasteClipboardContentToTrackAtPlayhead();
}

void TracksView::sliceButtonReleased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this) {
            if (midiCommandManager.isControlDown)
                viewModel.mergeSelectedTracksClipsAtPlayhead();
            else
                viewModel.splitSelectedTracksClipAtPlayHead();
        }
}

void TracksView::plusButtonReleased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this) {
            viewModel.addTrack();
            shouldUpdateTrackColour = true;
        }
}

void TracksView::minusButtonReleased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.deleteSelectedTrack();
}

void TracksView::loopingChanged(bool looping) {
    repaint();
    resized();
}
void TracksView::loopInButtonReleased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.setLoopIn();
}

void TracksView::loopOutButtonReleased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.setLoopOut();
}

void TracksView::loopButtonReleased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.toggleLooping();
}

void TracksView::recordButtonReleased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this) {
            if (midiCommandManager.isControlDown) {
                auto result = viewModel.startTapeRecording();
                message =
                    (result.wasOk()
                         ? "Recording audio. Stop to finish."
                         : result.getErrorMessage());
            } else {
                viewModel.startRecording();
            }
        }
}

void TracksView::playButtonReleased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.startPlaying();
}

void TracksView::stopButtonReleased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this) {
            const bool wasTapeRecording = viewModel.isTapeRecording();
            viewModel.stopRecordingOrPlaying();
            if (wasTapeRecording)
                message =
                    ("Tape ready. Slice / Cut / Paste / Undo");
        }
}

void TracksView::selectedIndexChanged(int) {
    rebuildLanes();
    updateDeck();
}

void TracksView::isRecordingChanged(bool) { updateDeck(); }
void TracksView::isPlayingChanged(bool) { updateDeck(); }

void TracksView::itemsChanged() {
    if (shouldUpdateTrackColour) {
        viewModel.setSelectedTrackColour(appLookAndFeel.getRandomColour());
        shouldUpdateTrackColour = false;
    }
    rebuildLanes();
    updateDeck();
}

void TracksView::tracksViewTypeChanged(
    app_view_models::TracksListViewModel::TracksViewType) {
    rebuildLanes();
}
void TracksView::soloStateChanged(bool) { repaint(); }
void TracksView::muteStateChanged(bool) { repaint(); }

void TracksView::updateDeck() {
    auto track = dynamic_cast<tracktion::AudioTrack *>(
        viewModel.listViewModel.getSelectedItem());
    juce::String lifted;
    auto clipboard = tracktion::Clipboard::getInstance();
    if (auto clips =
            clipboard->getContentWithType<tracktion::Clipboard::Clips>()) {
        if (!clips->clips.empty()) {
            const auto &clip = clips->clips.front().state;
            lifted = clip.getProperty(tracktion::IDs::name).toString() + "/" +
                     juce::String(static_cast<double>(
                                      clip.getProperty(tracktion::IDs::length)),
                                  1) +
                     " s";
        }
    }
    auto &transport = edit.getTransport();
    deck.update(transport.getCurrentPosition(), transport.isPlaying(),
                transport.isRecording(),
                track != nullptr ? track->getName() : "Select a synth to begin",
                lifted);
}

void TracksView::timerCallback() {
    if (!isShowing())
        return;
    // Do not reserve screen space for negative tape time.
    camera.setCenter(juce::jmax(camera.getScope() / 2.0,
                              edit.getTransport().getCurrentPosition()));
    for (auto lane : lanes)
        lane->resized();
    updateDeck();
    repaint();
}

void TracksView::undoButtonReleased() {
    if (isShowing() && midiCommandManager.getFocusedComponent() == this)
        viewModel.undo();
}
