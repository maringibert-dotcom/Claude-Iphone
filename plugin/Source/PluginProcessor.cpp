#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace masala;

juce::AudioProcessorValueTreeState::ParameterLayout MasalaProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < PID::COUNT; i++)
    {
        const auto& d = kParams[i];
        const juce::ParameterID pid { d.id, 1 };
        switch (d.type)
        {
            case ParamType::Float:
            {
                juce::NormalisableRange<float> range (d.min, d.max);
                if (d.logSkew)
                {
                    range = juce::NormalisableRange<float> (
                        d.min, d.max,
                        [] (float a, float b, float n) { return a * std::pow (b / a, n); },
                        [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); });
                }
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name, range, d.def));
                break;
            }
            case ParamType::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, d.name, (int) d.min, (int) d.max, (int) d.def));
                break;
            case ParamType::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f));
                break;
            case ParamType::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name, juce::StringArray::fromTokens (d.choices, "|", ""), (int) d.def));
                break;
        }
    }
    return layout;
}

MasalaProcessor::MasalaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MASALA", createLayout()),
      presets (apvts),
      cloud (presets)
{
    for (int i = 0; i < PID::COUNT; i++) raw[(size_t) i] = apvts.getRawParameterValue (kParams[i].id);
    presets.loadInitial();
}

bool MasalaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MasalaProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

void MasalaProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel()) engine.bend = (m.getPitchWheelValue() - 8192) / 8192.0;
    else if (m.isController() && m.getControllerNumber() == 1) engine.modWheel = m.getControllerValue() / 127.0;
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
}

void MasalaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    for (int i = 0; i < PID::COUNT; i++) engine.p[i] = raw[(size_t) i]->load();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) engine.bpm = juce::jlimit (20.0, 400.0, *bpm);

    if (panic.exchange (false)) engine.allNotesOff();

    buffer.clear();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, buffer.getNumSamples(), meta.samplePosition);
        if (t > pos) { engine.process (L + pos, R + pos, t - pos); pos = t; }
        handleMidi (meta.getMessage());
    }
    if (pos < buffer.getNumSamples()) engine.process (L + pos, R + pos, buffer.getNumSamples() - pos);

    if (auto* m = engine.lastMod())
    {
        for (int d = 0; d < kNumModDests; d++) liveMod[(size_t) d].store (m[d], std::memory_order_relaxed);
        liveModValid = true;
    }
    else liveModValid = false;
}

juce::AudioProcessorEditor* MasalaProcessor::createEditor() { return new MasalaEditor (*this); }

void MasalaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    presets.writeMeta (state);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void MasalaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            presets.readMeta (tree);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MasalaProcessor(); }
