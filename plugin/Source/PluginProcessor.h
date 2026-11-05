#pragma once

#include <JuceHeader.h>
#include "ParamDefs.h"
#include "dsp/Engine.h"
#include "PresetManager.h"
#include "CloudClient.h"

class MasalaProcessor : public juce::AudioProcessor
{
public:
    MasalaProcessor();
    ~MasalaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Masala"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return presets.currentName(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    CloudClient cloud;

    // modulations en direct (pour l'interface) — écrites par le thread audio
    std::array<std::atomic<float>, masala::kNumModDests> liveMod {};
    std::atomic<bool> liveModValid { false };
    std::atomic<bool> panic { false };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    masala::Engine engine;
    std::array<std::atomic<float>*, masala::PID::COUNT> raw {};
    void handleMidi (const juce::MidiMessage& m);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasalaProcessor)
};
