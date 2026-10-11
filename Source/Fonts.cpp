#include "Fonts.h"

#include <BinaryData.h>

namespace mnArpee::fonts
{
juce::Typeface::Ptr montserrat()
{
    static const juce::Typeface::Ptr face = juce::Typeface::createSystemTypefaceFor(
        BinaryData::Montserrat_ttf, (size_t) BinaryData::Montserrat_ttfSize);
    return face;
}

juce::Typeface::Ptr robotoCondensed()
{
    static const juce::Typeface::Ptr face = juce::Typeface::createSystemTypefaceFor(
        BinaryData::RobotoCondensed_ttf, (size_t) BinaryData::RobotoCondensed_ttfSize);
    return face;
}

juce::Font text(float height, bool bold)
{
    juce::Font f(juce::FontOptions(robotoCondensed()).withHeight(height));
    return bold ? f.boldened() : f;
}

juce::Font title(float height)
{
    return juce::Font(juce::FontOptions(montserrat()).withHeight(height)).boldened();
}
} // namespace mnArpee::fonts
