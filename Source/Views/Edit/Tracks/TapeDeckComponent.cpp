#include "TapeDeckComponent.h"
#include <cmath>

void TapeDeckComponent::update(double seconds, bool isPlaying, bool isRecording,
                               const juce::String &selectedTrack,
                               const juce::String &lifted) {
    const double now = juce::Time::getMillisecondCounterHiRes() * .001;
    const double elapsed =
        previousTick == 0 ? 0 : juce::jlimit(0.0, .1, now - previousTick);
    const double travel =
        isPlaying || isRecording
            ? elapsed
            : juce::jlimit(-.4, .4, seconds - previousPosition);
    angle = std::fmod(angle + static_cast<float>(travel) * 2.4f,
                      juce::MathConstants<float>::twoPi);
    previousPosition = seconds;
    previousTick = now;
    position = seconds;
    playing = isPlaying;
    recording = isRecording;
    trackName = selectedTrack;
    clipboard = lifted;
    repaint();
}

void TapeDeckComponent::drawReel(juce::Graphics &g, juce::Point<float> centre,
                                 float radius, float rotation,
                                 float tapeRadius) {
    auto circle =
        juce::Rectangle<float>(2 * radius, 2 * radius).withCentre(centre);
    g.setColour(TapeStyle::panel);
    g.fillEllipse(circle);
    g.setColour(TapeStyle::line);
    g.drawEllipse(circle, 1.0f);
    g.setColour(juce::Colour(0xff52636a));
    g.fillEllipse(circle.reduced(radius * (1 - tapeRadius)));
    g.setColour(TapeStyle::background);
    g.fillEllipse(circle.reduced(radius * .46f));
    for (int i = 0; i < 3; ++i) {
        const float a = rotation + i * juce::MathConstants<float>::twoPi / 3;
        const auto point =
            centre +
            juce::Point<float>(std::cos(a), std::sin(a)) * (radius * .32f);
        auto slot = juce::Rectangle<float>(radius * .16f, radius * .42f)
                        .withCentre(point);
        juce::Path spoke;
        spoke.addRoundedRectangle(slot, radius * .07f);
        g.setColour(TapeStyle::ink.withAlpha(.88f));
        g.fillPath(spoke, juce::AffineTransform::rotation(
                              a - juce::MathConstants<float>::halfPi, point.x,
                              point.y));
    }
    g.setColour(TapeStyle::line);
    g.fillEllipse(centre.x - radius * .14f, centre.y - radius * .14f,
                  radius * .28f, radius * .28f);
    g.setColour(TapeStyle::ink);
    g.fillEllipse(centre.x - 2, centre.y - 2, 4, 4);
}

void TapeDeckComponent::paint(juce::Graphics &g) {
    const float width = static_cast<float>(getWidth());
    const float height = static_cast<float>(getHeight());
    const float radius = juce::jmin(height * .32f, width * .09f);
    const juce::Point<float> left(width * .055f, height * .45f);
    const juce::Point<float> right(width * .17f, height * .45f);
    const auto accent = recording ? TapeStyle::red : TapeStyle::green;

    const float guideY = height - 16;
    juce::Path tape;
    tape.startNewSubPath(left.x - radius * .72f, left.y + radius * .52f);
    tape.lineTo(left.x - radius * .28f, guideY);
    tape.lineTo(right.x + radius * .28f, guideY);
    tape.lineTo(right.x + radius * .72f, right.y + radius * .52f);
    g.setColour(playing || recording ? accent.withAlpha(.8f)
                                     : TapeStyle::muted);
    g.strokePath(tape, juce::PathStrokeType(1.5f));
    g.setColour(TapeStyle::line);
    g.fillRoundedRectangle((left.x + right.x) * .5f - 14, guideY - 7, 28, 13,
                           3);
    g.setColour(accent);
    g.fillRect((left.x + right.x) * .5f - 4, guideY - 6, 8.0f, 2.0f);

    const float transferred =
        static_cast<float>(juce::jlimit(0.0, 1.0, position / 360.0));
    drawReel(g, left, radius, angle, .91f - transferred * .26f);
    drawReel(g, right, radius, angle * 1.13f, .65f + transferred * .26f);

    const int textX = juce::roundToInt(width * .28f);
    const int statusX = juce::roundToInt(width * .78f);
    g.setColour(TapeStyle::ink);
    g.setFont(TapeStyle::rowFont(46));
    g.drawText(trackName, textX, 0, getWidth() - textX, 36,
               juce::Justification::centredLeft, true);
    g.setFont(TapeStyle::rowFont(80));
    g.drawText(TapeStyle::time(position), textX, 36, statusX - textX - 8, 60,
               juce::Justification::centredLeft);
    g.setColour(playing || recording ? accent : TapeStyle::muted);
    g.setFont(TapeStyle::rowFont(34));
    g.drawText(recording ? "Recording" : playing ? "Playing" : "Stopped",
               statusX, 36, getWidth() - statusX, 30,
               juce::Justification::centredLeft);
    if (clipboard.isNotEmpty()) {
        g.setColour(TapeStyle::orange);
        g.drawText("Lift " + clipboard.fromLastOccurrenceOf("/", false, false).trim(),
                   statusX, 66, getWidth() - statusX, 30,
                   juce::Justification::centredLeft, true);
    }
}
