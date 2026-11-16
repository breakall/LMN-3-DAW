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
    const float radius = juce::jmin(height * .31f, width * .075f);
    const juce::Point<float> left(width * .16f, height * .59f);
    const juce::Point<float> right(width * .38f, height * .59f);
    const auto accent = recording ? TapeStyle::red : TapeStyle::green;

    g.setFont(TapeStyle::mono(12));
    g.setColour(TapeStyle::ink);
    g.drawText("LMN / TAPE", 0, 0, getWidth() / 2, 20,
               juce::Justification::left);
    g.setColour(TapeStyle::muted);
    g.drawText("SOUND ON TAPE", getWidth() / 2, 0, getWidth() / 2, 20,
               juce::Justification::right);

    const float guideY = height - 12;
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

    const int textX = juce::roundToInt(width * .55f);
    const int textWidth = getWidth() - textX;
    const int statusY = juce::roundToInt(height * .26f);
    g.setColour(playing || recording ? accent : TapeStyle::muted);
    g.fillEllipse(static_cast<float>(textX), static_cast<float>(statusY + 5), 6,
                  6);
    g.setFont(TapeStyle::mono(11));
    g.drawText(recording ? "RECORDING"
               : playing ? "PLAYING"
                         : "STOPPED",
               textX + 15, statusY, textWidth - 15, 17,
               juce::Justification::left);
    g.setColour(TapeStyle::ink);
    g.setFont(TapeStyle::mono(juce::jlimit(20.0f, 34.0f, width * .042f)));
    g.drawText(TapeStyle::time(position), textX, statusY + 20, textWidth, 40,
               juce::Justification::left);
    g.setFont(TapeStyle::mono(11));
    g.setColour(TapeStyle::muted);
    g.drawText(trackName, textX, statusY + 64, textWidth, 18,
               juce::Justification::left);
    if (clipboard.isNotEmpty()) {
        g.setColour(TapeStyle::orange);
        g.drawText("LIFTED  " + clipboard, textX, statusY + 84, textWidth, 18,
                   juce::Justification::left);
    }
}
