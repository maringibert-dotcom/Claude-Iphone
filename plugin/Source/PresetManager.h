#pragma once

#include <JuceHeader.h>
#include "ParamDefs.h"

// Presets MASALA : même format JSON (.masala) que l'appli téléphone.
//  - Usine : intégrés au plugin (shared/presets/factory.json)
//  - Mes presets : Documents/epices/Masala/Presets/User
//  - Cloud : Documents/epices/Masala/Presets/Cloud (synchronisés avec le compte)
struct PresetInfo
{
    enum class Source { Factory, User, Cloud };
    juce::String id, name, category;
    juce::var json;
    juce::File file;
    Source source = Source::Factory;
};

class PresetManager : public juce::ChangeBroadcaster
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState&);

    static juce::File rootDir();
    static juce::File userDir() { return rootDir().getChildFile ("User"); }
    static juce::File cloudDir() { return rootDir().getChildFile ("Cloud"); }

    void loadInitial();
    void rescan();
    const std::vector<PresetInfo>& all() const { return presets; }

    bool load (int index);
    bool loadJson (const juce::var& preset);
    void step (int dir);
    int currentIndex() const;

    juce::var serialize (const juce::String& id, const juce::String& name, const juce::String& category) const;
    PresetInfo saveUser (const juce::String& name, const juce::String& category, bool toCloudFolder);
    bool importFile (const juce::File& f);

    juce::String currentName() const { return name; }
    juce::String currentCategory() const { return category; }
    juce::String currentId() const { return id; }
    juce::String macroName (int i) const { return macros[i]; }

    void writeMeta (juce::ValueTree& state) const;
    void readMeta (const juce::ValueTree& state);

    static juce::String makeId();
    static bool writeJson (const juce::File& f, const juce::var& v);

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<PresetInfo> presets;
    juce::String id, name { "Init" }, category { "Leads & Pads" };
    juce::String macros[4] { "Macro 1", "Macro 2", "Macro 3", "Macro 4" };
    void scanFolder (const juce::File& dir, PresetInfo::Source src);
};
