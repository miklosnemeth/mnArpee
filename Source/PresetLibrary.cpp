#include "PresetLibrary.h"

#include <algorithm>

namespace mnArpee
{

namespace
{
const char* stepTypeName(StepType t)
{
    switch (t)
    {
        case StepType::Off:    return "off";
        case StepType::Rest:   return "rest";
        case StepType::Tie:    return "tie";
        case StepType::Random: return "random";
        default:               return "normal";
    }
}

StepType stepTypeFromName(const juce::String& s)
{
    if (s == "off") return StepType::Off;
    if (s == "rest") return StepType::Rest;
    if (s == "tie") return StepType::Tie;
    if (s == "random") return StepType::Random;
    return StepType::Normal;
}

SyncRate syncRateFromName(const juce::String& s, SyncRate fallback)
{
    for (int i = 0; i < numSyncRates; ++i)
        if (s == syncRateName((SyncRate) i))
            return (SyncRate) i;
    return fallback;
}

std::unique_ptr<juce::XmlElement> wrap(const Preset& preset)
{
    auto root = std::make_unique<juce::XmlElement>("mnArpeePreset");
    root->setAttribute("version", 1);
    root->addChildElement(presetToXml(preset).release());
    return root;
}

bool writeFile(const Preset& preset, const juce::File& file)
{
    return wrap(preset)->writeTo(file);
}

juce::String legalFileStem(const juce::String& name)
{
    auto stem = juce::File::createLegalFileName(name).trim();
    return stem.isEmpty() ? juce::String("Preset") : stem;
}
} // namespace

std::unique_ptr<juce::XmlElement> presetToXml(const Preset& preset)
{
    auto xml = std::make_unique<juce::XmlElement>("Preset");
    const auto& p = preset.program;
    xml->setAttribute("name", preset.name);
    xml->setAttribute("group", preset.group);
    xml->setAttribute("author", preset.author);
    xml->setAttribute("tags", preset.tags);
    xml->setAttribute("rate", syncRateName(p.sync));
    xml->setAttribute("gate", p.gatePercent);
    xml->setAttribute("swing", p.swingPercent);
    xml->setAttribute("octave", p.octaveShift);
    xml->setAttribute("transpose", p.transposeOct);
    xml->setAttribute("steps", p.numSteps);
    for (const auto& s : p.steps)
    {
        auto* step = xml->createNewChildElement("Step");
        step->setAttribute("type", stepTypeName(s.type));
        step->setAttribute("key", s.keyIndex == rootKeyIndex ? juce::String("root") : juce::String(s.keyIndex + 1));
        step->setAttribute("scale", (int) s.scaleStep);
        step->setAttribute("octave", (int) s.octave);
        step->setAttribute("velocity", (int) s.velocity);
        step->setAttribute("gate", (int) s.gateScalePercent);
        step->setAttribute("chance", (int) s.chance);
    }
    return xml;
}

bool presetFromXml(const juce::XmlElement& xml, Preset& out)
{
    if (! xml.hasTagName("Preset"))
        return false;

    Preset p;
    p.name = xml.getStringAttribute("name").trim();
    if (p.name.isEmpty())
        return false;
    p.group = xml.getStringAttribute("group", "User");
    p.author = xml.getStringAttribute("author");
    p.tags = xml.getStringAttribute("tags").trim();

    auto& prog = p.program;
    prog = makeInitProgram();
    prog.name.set(p.name.toStdString());
    prog.sync = syncRateFromName(xml.getStringAttribute("rate"), prog.sync);
    prog.gatePercent = juce::jlimit(1, 125, xml.getIntAttribute("gate", prog.gatePercent));
    prog.swingPercent = juce::jlimit(-50, 50, xml.getIntAttribute("swing", prog.swingPercent));
    prog.octaveShift = juce::jlimit(-2, 2, xml.getIntAttribute("octave", prog.octaveShift));
    prog.transposeOct = juce::jlimit(-3, 3, xml.getIntAttribute("transpose", prog.transposeOct));
    prog.numSteps = juce::jlimit(0, maxSteps, xml.getIntAttribute("steps", prog.numSteps));

    int i = 0;
    for (const auto* e : xml.getChildWithTagNameIterator("Step"))
    {
        if (i >= maxSteps)
            break;
        auto& s = prog.steps[(size_t) i++];
        s.type = stepTypeFromName(e->getStringAttribute("type"));
        const auto key = e->getStringAttribute("key");
        s.keyIndex = key == "root" ? rootKeyIndex
                                   : (std::int8_t) juce::jlimit(0, maxInputKeys - 1, key.getIntValue() - 1);
        s.scaleStep = (std::int8_t) juce::jlimit(-12, 12, e->getIntAttribute("scale", 0));
        s.octave = (std::int8_t) juce::jlimit(-3, 3, e->getIntAttribute("octave", 0));
        s.velocity = (std::uint8_t) juce::jlimit(0, 127, e->getIntAttribute("velocity", 96));
        s.gateScalePercent = (std::uint16_t) juce::jlimit(10, 800, e->getIntAttribute("gate", 100));
        s.chance = (std::uint8_t) juce::jlimit(1, 100, e->getIntAttribute("chance", 100));
    }

    p.factory = false;
    out = p;
    return true;
}

juce::StringArray presetStyles(const Preset& preset)
{
    juce::StringArray styles;
    styles.addTokens(preset.tags, ",", "");
    styles.trim();
    styles.removeEmptyStrings();
    return styles;
}

juce::String presetSnapshot(const Preset& preset)
{
    return presetToXml(preset)->toString(juce::XmlElement::TextFormat().singleLine());
}

std::vector<Preset> makeFactoryPresets()
{
    std::vector<Preset> out;
    for (const auto& pattern : makeFactoryPatterns())
    {
        Preset p;
        p.program = pattern.program;
        p.name = juce::String::fromUTF8(p.program.name.chars.data());
        p.group = pattern.group;
        p.author = "mnArpee";
        p.tags = pattern.styles;
        p.factory = true;
        out.push_back(std::move(p));
    }
    return out;
}

//==============================================================================
juce::File PresetLibrary::defaultRoot()
{
    const auto overridePath = juce::SystemStats::getEnvironmentVariable("MNARPEE_PRESET_DIR", {});
    if (overridePath.isNotEmpty())
        return juce::File(overridePath);
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("MNAudio")
        .getChildFile("mnArpee");
}

PresetLibrary::PresetLibrary(juce::File rootDir_) : rootDir(std::move(rootDir_)) {}

void PresetLibrary::scan(const juce::File& dir, bool factory, std::vector<Preset>& into) const
{
    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.xml");
    files.sort();
    std::vector<Preset> found;
    for (const auto& f : files)
    {
        auto xml = juce::parseXML(f);
        if (xml == nullptr || ! (xml->hasTagName("mnArpeePreset") || xml->hasTagName("Preset")))
            continue;
        const auto* element = xml->hasTagName("Preset") ? xml.get() : xml->getChildByName("Preset");
        Preset p;
        if (element == nullptr || ! presetFromXml(*element, p))
            continue;
        p.factory = factory;
        p.file = f;
        found.push_back(std::move(p));
    }
    if (! factory)
        std::stable_sort(found.begin(), found.end(), [](const Preset& a, const Preset& b)
                         { return a.name.compareIgnoreCase(b.name) < 0; });
    for (auto& p : found)
        into.push_back(std::move(p));
}

void PresetLibrary::refresh()
{
    factoryDir().createDirectory();
    userDir().createDirectory();

    // Factory files: rewritten when they differ (the plugin owns this folder).
    const auto factoryPresets = makeFactoryPresets();
    juce::StringArray expected;
    for (size_t i = 0; i < factoryPresets.size(); ++i)
    {
        const auto fileName = juce::String::formatted("%02d ", (int) i + 1) + legalFileStem(factoryPresets[i].name) + ".xml";
        expected.add(fileName);
        const auto file = factoryDir().getChildFile(fileName);
        const auto text = wrap(factoryPresets[i])->toString();
        if (! file.existsAsFile() || file.loadFileAsString() != text)
            file.replaceWithText(text);
    }
    for (const auto& f : factoryDir().findChildFiles(juce::File::findFiles, false, "*.xml"))
        if (! expected.contains(f.getFileName()))
            f.deleteFile();

    list.clear();
    scan(factoryDir(), true, list);
    scan(userDir(), false, list);
}

int PresetLibrary::indexOfFile(const juce::File& file) const
{
    if (file == juce::File())
        return -1;
    for (int i = 0; i < size(); ++i)
        if (list[(size_t) i].file == file)
            return i;
    return -1;
}

int PresetLibrary::indexOf(const Preset& preset) const
{
    if (const int byFile = indexOfFile(preset.file); byFile >= 0)
        return byFile;
    for (int i = 0; i < size(); ++i)
        if (list[(size_t) i].name == preset.name && list[(size_t) i].group == preset.group)
            return i;
    return -1;
}

bool PresetLibrary::nameExists(const juce::String& name) const
{
    return std::any_of(list.begin(), list.end(), [&](const Preset& p) { return p.name.equalsIgnoreCase(name); });
}

juce::String PresetLibrary::freeName(const juce::String& wanted) const
{
    const auto base = wanted.trim().isEmpty() ? juce::String("New Arp") : wanted.trim();
    if (! nameExists(base))
        return base;
    for (int n = 2;; ++n)
    {
        const auto candidate = base + " " + juce::String(n);
        if (! nameExists(candidate))
            return candidate;
    }
}

juce::File PresetLibrary::uniqueUserFile(const juce::String& name) const
{
    const auto stem = legalFileStem(name);
    auto file = userDir().getChildFile(stem + ".xml");
    for (int n = 2; file.exists(); ++n)
        file = userDir().getChildFile(stem + " " + juce::String(n) + ".xml");
    return file;
}

bool PresetLibrary::save(Preset& preset)
{
    if (preset.factory || preset.name.trim().isEmpty())
        return false;
    userDir().createDirectory();
    preset.name = preset.name.trim();
    preset.program.name.set(preset.name.toStdString());
    if (preset.group.isEmpty() || preset.group == "Factory")
        preset.group = "User";
    if (preset.file == juce::File())
        preset.file = uniqueUserFile(preset.name);
    if (! writeFile(preset, preset.file))
        return false;
    refresh();
    return true;
}

bool PresetLibrary::rename(const Preset& preset, const juce::String& newName, Preset& renamed)
{
    const auto name = newName.trim();
    if (preset.factory || name.isEmpty() || preset.file == juce::File())
        return false;
    renamed = preset;
    renamed.name = name;
    renamed.program.name.set(name.toStdString());

    // The file follows the name, unless that would only differ by a number.
    const auto oldFile = preset.file;
    auto newFile = userDir().getChildFile(legalFileStem(name) + ".xml");
    if (newFile != oldFile)
        newFile = uniqueUserFile(name);
    renamed.file = newFile;
    if (! writeFile(renamed, newFile))
        return false;
    if (newFile != oldFile)
        oldFile.deleteFile();
    refresh();
    return true;
}

bool PresetLibrary::remove(const Preset& preset)
{
    if (preset.factory || preset.file == juce::File() || ! preset.file.existsAsFile())
        return false;
    // To the recycle bin, except when the folder is overridden (the tests): those files just go.
    const bool overridden = juce::SystemStats::getEnvironmentVariable("MNARPEE_PRESET_DIR", {}).isNotEmpty();
    if (overridden || ! preset.file.moveToTrash())
        preset.file.deleteFile();
    refresh();
    return true;
}

bool PresetLibrary::duplicate(const Preset& preset, Preset& copy)
{
    copy = preset;
    copy.factory = false;
    copy.group = "User";
    copy.file = {};
    copy.name = freeName(preset.name + " copy");
    return save(copy);
}

int PresetLibrary::importFile(const juce::File& file, juce::String* firstName)
{
    auto xml = juce::parseXML(file);
    if (xml == nullptr)
        return 0;

    std::vector<const juce::XmlElement*> elements;
    if (xml->hasTagName("Preset"))
        elements.push_back(xml.get());
    else if (xml->hasTagName("mnArpeePreset") || xml->hasTagName("mnArpeeBank"))
        for (const auto* e : xml->getChildWithTagNameIterator("Preset"))
            elements.push_back(e);

    int count = 0;
    for (const auto* e : elements)
    {
        Preset p;
        if (! presetFromXml(*e, p))
            continue;
        p.factory = false;
        p.file = {};
        if (p.group.isEmpty() || p.group == "Factory")
            p.group = "User";
        p.name = freeName(p.name);
        if (save(p))
        {
            if (count == 0 && firstName != nullptr)
                *firstName = p.name;
            ++count;
        }
    }
    return count;
}

bool PresetLibrary::exportPreset(const Preset& preset, const juce::File& file) const
{
    return writeFile(preset, file);
}

} // namespace mnArpee
