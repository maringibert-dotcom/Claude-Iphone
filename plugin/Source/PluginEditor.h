#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace epices
{
// palette « épices »
const juce::Colour poivre   { 0xff140f0b };
const juce::Colour torref   { 0xff1d1611 };
const juce::Colour cannelle { 0xff2a1f17 };
const juce::Colour cacao    { 0xff3a2b20 };
const juce::Colour line     { 0xff4a3727 };
const juce::Colour creme    { 0xfff4e7d1 };
const juce::Colour sable    { 0xffbfa88a };
const juce::Colour muted    { 0xff8a7560 };
const juce::Colour safran   { 0xfff2a516 };
const juce::Colour paprika  { 0xffe0522a };
const juce::Colour piment   { 0xffc42b2b };
const juce::Colour curcuma  { 0xffe8c020 };
const juce::Colour cardamome{ 0xff9bb35f };
const juce::Colour badiane  { 0xff8a6bd1 };

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool over, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f, juce::Font::bold)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (13.0f, juce::Font::bold)); }
};
} // namespace epices

// Écran d'une table d'onde (avec la position modulée en direct)
class WaveView : public juce::Component
{
public:
    WaveView (MasalaProcessor& p, bool oscA) : proc (p), isA (oscA) {}
    void paint (juce::Graphics&) override;

private:
    MasalaProcessor& proc;
    bool isA;
};

// Groupe de contrôles (une « carte » de l'interface)
class ParamGroup : public juce::Component
{
public:
    ParamGroup (MasalaProcessor&, juce::String title, juce::Colour accent, juce::StringArray ids, juce::StringArray labels = {});
    void paint (juce::Graphics&) override;
    void resized() override;
    int preferredWidth() const;
    void addTop (std::unique_ptr<juce::Component> c, int height) { top = std::move (c); topH = height; addAndMakeVisible (*top); }

private:
    juce::String title;
    juce::Colour accent;
    std::unique_ptr<juce::Component> top;
    int topH = 0;
    struct Item
    {
        std::unique_ptr<juce::Component> comp;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
        bool wide = false;
    };
    std::vector<Item> items;
};

// Page = grille de groupes
class ParamPage : public juce::Component
{
public:
    void add (std::unique_ptr<ParamGroup> g) { addAndMakeVisible (*g); groups.push_back (std::move (g)); }
    void resized() override;

private:
    std::vector<std::unique_ptr<ParamGroup>> groups;
};

class MasalaEditor : public juce::AudioProcessorEditor,
                     private juce::ChangeListener,
                     private juce::Timer
{
public:
    explicit MasalaEditor (MasalaProcessor&);
    ~MasalaEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MasalaProcessor& proc;
    epices::LookAndFeel lnf;

    juce::TextButton presetBtn, prevBtn { "<" }, nextBtn { ">" }, saveBtn { "Enregistrer" }, accountBtn { "Compte" };
    juce::Label status;
    juce::Slider macro[4];
    juce::Label macroLabel[4];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> macroAtt[4];
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    WaveView* waveA = nullptr;
    WaveView* waveB = nullptr;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshPreset();
    void showPresetMenu();
    void showAccountMenu();
    void savePreset();
    void setStatus (const juce::String&);
    std::unique_ptr<juce::AlertWindow> dialog;
    std::unique_ptr<juce::FileChooser> chooser;
};
