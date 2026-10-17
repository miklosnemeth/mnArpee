#pragma once

#include <JuceHeader.h>

#include "PatternData.h"

#include <memory>
#include <vector>

//==============================================================================
// Presets: one arp pattern (a Program) with a name, group and author, stored as an XML file.
//
//   <mnArpeePreset version="1">
//     <Preset name="..." group="..." author="..." tags="..." rate="1/16" gate="80" swing="0" octave="0"
//             transpose="0" steps="16">
//       <Step type="normal" key="1" scale="0" octave="0" velocity="96" gate="100" chance="100" ratchet="1"/>
//       ... one Step per stored step
//     </Preset>
//   </mnArpeePreset>
//
// A bank file (<mnArpeeBank>) holds several <Preset> elements; Import accepts both.
//
// The library lives in <root>/Factory (written by the plugin, read only) and <root>/User
// (one file per preset, shared by every project and plugin instance).
//==============================================================================
namespace mnArpee
{

struct Preset
{
    juce::String name;
    juce::String group = "User";
    juce::String author;
    juce::String tags;     // musical styles, comma separated ("House,Trance")
    Program program;       // program.name mirrors name
    bool factory = false;
    bool favorite = false; // a star set in the browser; kept in favorites.txt, not in the preset file
    juce::File file;       // empty while the preset has not been saved
};

std::unique_ptr<juce::XmlElement> presetToXml(const Preset& preset);
// Reads a <Preset> element; false when it is not one.
bool presetFromXml(const juce::XmlElement& xml, Preset& out);
// The preset's content as text (no file), used to tell whether it has unsaved changes.
juce::String presetSnapshot(const Preset& preset);
// The styles of a preset as a list ("House, Trance" -> House, Trance).
juce::StringArray presetStyles(const Preset& preset);
// The factory presets, in order.
std::vector<Preset> makeFactoryPresets();

class PresetLibrary
{
public:
    // MNARPEE_PRESET_DIR overrides the default <Documents>/MNAudio/mnArpee (used by the tests).
    static juce::File defaultRoot();

    explicit PresetLibrary(juce::File rootDir = defaultRoot());

    juce::File root() const { return rootDir; }
    juce::File userDir() const { return rootDir.getChildFile("User"); }
    juce::File factoryDir() const { return rootDir.getChildFile("Factory"); }

    // Writes the factory files that are missing or out of date, then scans both folders.
    // Factory presets come first, in their order; user presets follow, sorted by name.
    void refresh();
    const std::vector<Preset>& presets() const { return list; }
    int size() const { return (int) list.size(); }
    int indexOfFile(const juce::File& file) const;
    int indexOf(const Preset& preset) const; // by file, else by group and name; -1 if absent

    // Writes the preset into User (a new file when it has none) and refreshes the list.
    bool save(Preset& preset);
    // Renames a user preset (its file gets the new name too). `renamed` receives the result.
    bool rename(const Preset& preset, const juce::String& newName, Preset& renamed);
    // Moves a user preset's file to the recycle bin.
    bool remove(const Preset& preset);
    // A user copy of the preset with a free name ("Name copy", "Name copy 2", ...).
    bool duplicate(const Preset& preset, Preset& copy);
    // Saves every preset in a preset or bank file as a user preset; returns how many.
    int importFile(const juce::File& file, juce::String* firstName = nullptr);
    bool exportPreset(const Preset& preset, const juce::File& file) const;

    // Stars: shared by all projects, kept by preset name in <root>/favorites.txt.
    void setFavorite(const juce::String& name, bool favorite);

    // A name no user preset has yet ("New Arp", "New Arp 2", ...).
    juce::String freeName(const juce::String& wanted) const;
    bool nameExists(const juce::String& name) const;

private:
    juce::File uniqueUserFile(const juce::String& name) const;
    void scan(const juce::File& dir, bool factory, std::vector<Preset>& into) const;

    juce::File favoritesFile() const { return rootDir.getChildFile("favorites.txt"); }

    juce::File rootDir;
    juce::StringArray favoriteNames;
    std::vector<Preset> list;
};

} // namespace mnArpee
