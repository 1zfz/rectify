#include "PluginProcessor.h"
#include <cmath>

//==============================================================================
RectifyAudioProcessor::RectifyAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    modeParam   = apvts.getRawParameterValue ("mode");
    driveParam  = apvts.getRawParameterValue ("drive");
    amountParam = apvts.getRawParameterValue ("amount");
    mixParam    = apvts.getRawParameterValue ("mix");
    outputParam = apvts.getRawParameterValue ("output");
}

//==============================================================================
// Aqui voce declara os knobs do plugin. O host (DAW) automatiza esses IDs.
juce::AudioProcessorValueTreeState::ParameterLayout RectifyAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Modo",
        StringArray { "Onda completa", "Meia onda +", "Meia onda -" }, 0));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "drive", 1 }, "Drive",
        NormalisableRange<float> (0.0f, 40.0f, 0.1f), 12.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "amount", 1 }, "Retificacao",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, "Saida",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

//==============================================================================
void RectifyAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate,
                                  (juce::uint32) samplesPerBlock,
                                  (juce::uint32) getTotalNumOutputChannels() };

    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();

    mixer.prepare (spec);
    mixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    mixer.setWetLatency (oversampling.getLatencyInSamples());
    setLatencySamples (juce::roundToInt (oversampling.getLatencyInSamples()));

    const double osRate = sampleRate * (double) oversampling.getOversamplingFactor();

    driveGain.reset (sampleRate, 0.02);
    outputGain.reset (sampleRate, 0.02);
    amount.reset (osRate, 0.02);

    driveGain.setCurrentAndTargetValue  (juce::Decibels::decibelsToGain (driveParam->load()));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    amount.setCurrentAndTargetValue     (amountParam->load() * 0.01f);

    // Filtro passa-alta de 1 polo em ~20 Hz: y[n] = x[n] - x[n-1] + R*y[n-1]
    dcR = 1.0f - (juce::MathConstants<float>::twoPi * 20.0f / (float) sampleRate);
    std::fill (std::begin (xPrev), std::end (xPrev), 0.0f);
    std::fill (std::begin (yPrev), std::end (yPrev), 0.0f);
}

bool RectifyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
void RectifyAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples  = buffer.getNumSamples();

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    // Le os parametros uma vez por bloco
    driveGain.setTargetValue  (juce::Decibels::decibelsToGain (driveParam->load()));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    amount.setTargetValue     (amountParam->load() * 0.01f);
    mixer.setWetMixProportion (mixParam->load() * 0.01f);
    const int mode = (int) modeParam->load();

    float* const* data = buffer.getArrayOfWritePointers();
    juce::dsp::AudioBlock<float> block (buffer);

    // Guarda o sinal limpo pro dry/wet
    mixer.pushDrySamples (block);

    // 1) Drive (ganho antes da retificacao)
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = driveGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            data[ch][i] *= g;
    }

    // 2) Retificacao com oversampling 4x
    auto osBlock = oversampling.processSamplesUp (block);
    const int osSamples = (int) osBlock.getNumSamples();
    const int osChannels = (int) osBlock.getNumChannels();

    for (int i = 0; i < osSamples; ++i)
    {
        const float a = amount.getNextValue(); // 0 = limpo, 1 = totalmente retificado

        for (int ch = 0; ch < osChannels; ++ch)
        {
            float* d = osBlock.getChannelPointer ((size_t) ch);
            const float x = d[i];

            float r;
            switch (mode)
            {
                case 0:  r = std::abs (x);          break; // onda completa
                case 1:  r = juce::jmax (x, 0.0f);  break; // meia onda (so positivo)
                default: r = juce::jmin (x, 0.0f);  break; // meia onda (so negativo)
            }

            // mistura entre sinal original e retificado, com soft clip de seguranca
            d[i] = std::tanh (x + a * (r - x));
        }
    }

    oversampling.processSamplesDown (block);

    // 3) Remove o DC gerado pela retificacao
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float x1 = xPrev[ch], y1 = yPrev[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float x = data[ch][i];
            const float y = x - x1 + dcR * y1;
            x1 = x;
            y1 = y;
            data[ch][i] = y;
        }

        xPrev[ch] = x1;
        yPrev[ch] = y1;
    }

    // 4) Dry/Wet e ganho de saida
    mixer.mixWetSamples (block);

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = outputGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            data[ch][i] *= g;
    }
}

//==============================================================================
// Editor generico: o JUCE desenha um knob/slider pra cada parametro sozinho.
juce::AudioProcessorEditor* RectifyAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void RectifyAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RectifyAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RectifyAudioProcessor();
}
