#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace rimac
{
using APVTS = juce::AudioProcessorValueTreeState;

static juce::String hzText (float hz)
{
    if (hz >= 1000.0f)
        return juce::String (hz / 1000.0f, 2) + " kHz";
    return juce::String (juce::roundToInt (hz)) + " Hz";
}

//==============================================================================
APVTS::ParameterLayout RimacAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::depth, 1 }, "Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.70f,
        juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
            [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));

    auto timeRange = juce::NormalisableRange<float> (0.1f, 500.0f, 0.1f);
    timeRange.setSkewForCentre (20.0f);

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::attack, 1 }, "Attack", timeRange, 5.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, v < 10.0f ? 1 : 0) + " ms"; })));

    auto relRange = juce::NormalisableRange<float> (5.0f, 2000.0f, 1.0f);
    relRange.setSkewForCentre (120.0f);

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::release, 1 }, "Release", relRange, 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, v < 100.0f ? 1 : 0) + " ms"; })));

    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ids::mode, 1 }, "Mode",
        juce::StringArray { "Soft", "Hard" }, 1));

    p.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ids::eqOn, 1 }, "Ecualizacion", true));

    auto freqRange = juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f);
    freqRange.setSkewForCentre (1000.0f);

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::eqFreq, 1 }, "EQ Freq", freqRange, 1200.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (
            [] (float v, int) { return hzText (v); })));

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::eqGain, 1 }, "EQ Gain",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), -6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 1) + " dB"; })));

    p.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ids::scOn, 1 }, "Sidechain", true));

    auto scRange = juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f);
    scRange.setSkewForCentre (2000.0f);

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::scFreq, 1 }, "Sidechain Freq", scRange, 2000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (
            [] (float v, int) { return hzText (v); })));

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::scSens, 1 }, "Sidechain Sens",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -24.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 1) + " dB"; })));

    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ids::processing, 1 }, "Procesamiento",
        juce::StringArray { "Mid", "Side" }, 0));

    p.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ids::bypass, 1 }, "Bypass", false));

    return { p.begin(), p.end() };
}

//==============================================================================
RimacAudioProcessor::RimacAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RIMAC", createLayout())
{
    auto get = [this] (const char* id)
    {
        return apvts.getRawParameterValue (id);
    };

    depthParam   = get (ids::depth);
    attackParam  = get (ids::attack);
    releaseParam = get (ids::release);
    modeParam    = get (ids::mode);
    eqOnParam    = get (ids::eqOn);
    eqFreqParam  = get (ids::eqFreq);
    eqGainParam  = get (ids::eqGain);
    scOnParam    = get (ids::scOn);
    scFreqParam  = get (ids::scFreq);
    scSensParam  = get (ids::scSens);
    procParam    = get (ids::processing);
    bypassParam  = get (ids::bypass);
}

void RimacAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples (SpectralSuppressor::latency);
}

void RimacAudioProcessor::releaseResources()
{
    engine.reset();
}

bool RimacAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& main = layouts.getMainOutputChannelSet();
    return main == juce::AudioChannelSet::stereo() || main == juce::AudioChannelSet::mono();
}

void RimacAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midi);

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.depth           = depthParam->load();
    engine.attackMs        = attackParam->load();
    engine.releaseMs       = releaseParam->load();
    engine.hardMode        = modeParam->load() > 0.5f;
    engine.eqEnabled       = eqOnParam->load() > 0.5f;
    engine.eqFreqHz        = eqFreqParam->load();
    engine.eqGainDb        = eqGainParam->load();
    engine.sidechainEnabled = scOnParam->load() > 0.5f;
    engine.sideFreqHz      = scFreqParam->load();
    engine.sideSensDb      = scSensParam->load();
    engine.processSide     = procParam->load() > 0.5f;
    engine.bypassed         = bypassParam->load() > 0.5f;

    const float* scL = numIn > 0 ? buffer.getReadPointer (0) : nullptr;
    const float* scR = numIn > 1 ? buffer.getReadPointer (1) : scL;

    engine.process (buffer.getArrayOfWritePointers(), numOut,
                    buffer.getNumSamples(), scL, scR);

    outputLevelDb.store (juce::Decibels::gainToDecibels (buffer.getMagnitude (0, buffer.getNumSamples()),
                                                          -100.0f),
                         std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* RimacAudioProcessor::createEditor()
{
    return new RimacAudioProcessorEditor (*this);
}

int RimacAudioProcessor::getNumPrograms() { return 3; }
int RimacAudioProcessor::getCurrentProgram() { return currentProgram; }

void RimacAudioProcessor::setCurrentProgram (int index)
{
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, index);

    switch (currentProgram)
    {
        case 0: // Default
            resetAllParameters();
            break;
        case 1: // Vocal focus
            apvts.getParameter (ids::depth)->setValueNotifyingHost (0.62f);
            apvts.getParameter (ids::mode)->setValueNotifyingHost (0.0f);
            apvts.getParameter (ids::attack)->setValueNotifyingHost (0.20f);
            apvts.getParameter (ids::release)->setValueNotifyingHost (0.40f);
            apvts.getParameter (ids::eqGain)->setValueNotifyingHost (0.55f);
            break;
        case 2: // Aggressive master
            apvts.getParameter (ids::depth)->setValueNotifyingHost (0.90f);
            apvts.getParameter (ids::mode)->setValueNotifyingHost (1.0f);
            apvts.getParameter (ids::attack)->setValueNotifyingHost (0.05f);
            apvts.getParameter (ids::release)->setValueNotifyingHost (0.25f);
            apvts.getParameter (ids::eqGain)->setValueNotifyingHost (0.35f);
            break;
        default: break;
    }
}

const juce::String RimacAudioProcessor::getProgramName (int index)
{
    static const juce::StringArray names { "Default Preset", "Vocal Tamer", "Aggressive" };
    return names[index % names.size()];
}

void RimacAudioProcessor::changeProgramName (int, const juce::String&) {}

void RimacAudioProcessor::resetAllParameters()
{
    for (auto* param : getParameters())
        param->setValueNotifyingHost (param->getDefaultValue());
}

//==============================================================================
void RimacAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RimacAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
float RimacAudioProcessor::getSpectrum (float* dest, int numBins) const
{
    engine.readSpectrum (dest, numBins);
    return 0.0f;
}

float RimacAudioProcessor::readParam (const char* id) const
{
    if (auto* v = apvts.getRawParameterValue (id))
        return v->load();
    return 0.0f;
}
} // namespace rimac

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new rimac::RimacAudioProcessor();
}
