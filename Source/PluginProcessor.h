#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class RectifyAudioProcessor : public juce::AudioProcessor
{
public:
    RectifyAudioProcessor();
    ~RectifyAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Rectify Distortion"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr size_t kOversamplingOrder = 2; // 2^2 = 4x

    // Ponteiros diretos pros parametros (leitura thread-safe no audio thread)
    std::atomic<float>* modeParam   = nullptr;
    std::atomic<float>* driveParam  = nullptr;
    std::atomic<float>* amountParam = nullptr;
    std::atomic<float>* mixParam    = nullptr;
    std::atomic<float>* outputParam = nullptr;

    // Suavizacao pra evitar "cliques" ao mexer nos knobs
    juce::SmoothedValue<float> driveGain, outputGain, amount;

    // Oversampling reduz o aliasing que a retificacao (quina aguda) gera
    juce::dsp::Oversampling<float> oversampling { 2, kOversamplingOrder,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };

    // Mistura dry/wet ja compensando a latencia do oversampling
    juce::dsp::DryWetMixer<float> mixer { 64 };

    // Bloqueador de DC (a retificacao cria offset de DC)
    float dcR = 0.997f;
    float xPrev[2] {}, yPrev[2] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RectifyAudioProcessor)
};
