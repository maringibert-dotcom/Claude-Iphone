#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace epices
{
// palette epices audio (DA officielle : orange #F2541B, noir #1E1B16)
const juce::Colour orange   { 0xfff2541b };
const juce::Colour orangeVif{ 0xffff7a45 };
const juce::Colour peche    { 0xfff7a072 };
const juce::Colour noir     { 0xff1e1b16 };
const juce::Colour encre    { 0xff151310 };
const juce::Colour noir2    { 0xff28241e };
const juce::Colour noir3    { 0xff36302a };
const juce::Colour line     { 0xff433b32 };
const juce::Colour papier   { 0xfff3ede4 };
const juce::Colour sable    { 0xffbdb2a3 };
const juce::Colour muted    { 0xff857a6c };

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
