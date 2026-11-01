#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

ShaperProcessor::ShaperProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    preParam = apvts.getRawParameterValue ("pre");
    postParam = apvts.getRawParameterValue ("post");
    mixParam = apvts.getRawParameterValue ("mix");
    dcParam = apvts.getRawParameterValue ("dc");

    for (size_t i = 0; i < localTable.size(); ++i)
        localTable[i] = -1.0f + 2.0f * (float) i / (float) ShapeCurve::tableSize;
}

juce::AudioProcessorValueTreeState::ParameterLayout ShaperProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "pre", 1 }, "Pre",
        NormalisableRange<float> (-24.0f, 120.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "post", 1 }, "Post",
        NormalisableRange<float> (-48.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "dc", 1 }, "DC Filter", false));

    return { params.begin(), params.end() };
}

void ShaperProcessor::prepareToPlay (double sampleRate, int)
{
    preGain.reset (sampleRate, 0.02);
    postGain.reset (sampleRate, 0.02);
    mix.reset (sampleRate, 0.02);

    preGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (preParam->load()));
    postGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (postParam->load()));
    mix.setCurrentAndTargetValue (mixParam->load() * 0.01f);

    dcCoefficient = 1.0f - juce::MathConstants<float>::twoPi * 20.0f / (float) sampleRate;
    std::fill (std::begin (xPrevious), std::end (xPrevious), 0.0f);
    std::fill (std::begin (yPrevious), std::end (yPrevious), 0.0f);
}

bool ShaperProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

float ShaperProcessor::shape (float x) const noexcept
{
    if (! std::isfinite (x))
        return 0.0f;

    const float position = (juce::jlimit (-1.0f, 1.0f, x) + 1.0f) * (0.5f * (float) ShapeCurve::tableSize);
    const int index = juce::jmin ((int) position, ShapeCurve::tableSize - 1);
    const float fraction = position - (float) index;
    const float a = localTable[(size_t) index];
    const float b = localTable[(size_t) index + 1];

    return a + fraction * (b - a);
}

void ShaperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    const int currentVersion = curve.getVersion();

    if (currentVersion != localVersion)
        curve.copyTable (localTable.data(), localVersion);

    preGain.setTargetValue (juce::Decibels::decibelsToGain (preParam->load()));
    postGain.setTargetValue (juce::Decibels::decibelsToGain (postParam->load()));
    mix.setTargetValue (mixParam->load() * 0.01f);
    const bool dcOn = dcParam->load() > 0.5f;

    float* const* data = buffer.getArrayOfWritePointers();

    for (int i = 0; i < numSamples; ++i)
    {
        const float pre = preGain.getNextValue();
        const float post = postGain.getNextValue();
        const float wetAmount = mix.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dry = data[ch][i];
            float wet = shape (dry * pre) * post;

            const float filtered = wet - xPrevious[ch] + dcCoefficient * yPrevious[ch];
            xPrevious[ch] = wet;
            yPrevious[ch] = filtered;

            if (dcOn)
                wet = filtered;

            data[ch][i] = dry + wetAmount * (wet - dry);
        }
    }
}

juce::AudioProcessorEditor* ShaperProcessor::createEditor()
{
    return new ShaperEditor (*this);
}

void ShaperProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    apvts.state.setProperty ("curve", curve.toString(), nullptr);
    apvts.state.setProperty ("smooth", curve.isSmooth(), nullptr);

    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ShaperProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            curve.setSmooth ((bool) apvts.state.getProperty ("smooth", false));
            curve.fromString (apvts.state.getProperty ("curve", juce::String()).toString());
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ShaperProcessor();
}
