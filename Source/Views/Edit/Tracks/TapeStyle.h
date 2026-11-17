#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace TapeStyle {
inline const juce::Colour background{0xff11171b};
inline const juce::Colour panel{0xff192227};
inline const juce::Colour line{0xff2d3b41};
inline const juce::Colour ink{0xffeeeede};
inline const juce::Colour muted{0xff82969e};
inline const juce::Colour blue{0xff6bbdeb};
inline const juce::Colour green{0xff80d4ad};
inline const juce::Colour orange{0xfff1a36f};
inline const juce::Colour red{0xfff56f65};

inline juce::Font mono(float size) {
    return juce::Font(juce::Font::getDefaultMonospacedFontName(), size,
                      juce::Font::plain);
}

// Match SimpleListItemView and ListTitle: type occupies 70% of its row.
inline juce::Font rowFont(float rowHeight) { return mono(rowHeight * .7f); }

inline juce::String time(double seconds) {
    const auto milliseconds = juce::jmax(0, juce::roundToInt(seconds * 1000));
    return juce::String::formatted("%02d:%02d.%03d", milliseconds / 60000,
                                   (milliseconds / 1000) % 60,
                                   milliseconds % 1000);
}
} // namespace TapeStyle
