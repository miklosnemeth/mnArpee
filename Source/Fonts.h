#pragma once

#include <JuceHeader.h>

//==============================================================================
// Bundled fonts (SIL Open Font License): Montserrat for the "mnArpee" title, Roboto
// Condensed for every other piece of text. Fonts are built from these typefaces
// explicitly, because JUCE resolves default fonts through the global LookAndFeel only.
//==============================================================================
namespace mnArpee::fonts
{
juce::Typeface::Ptr montserrat();
juce::Typeface::Ptr robotoCondensed();

// UI text.
juce::Font text(float height, bool bold = false);
// Header title.
juce::Font title(float height);
} // namespace mnArpee::fonts
