#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ShapeCurve.h"

class ShaperProcessor : public juce::AudioProcessor
{
public:
    ShaperProcessor();
    ~ShaperProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Rectify Waveshaper"; }
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

    ShapeCurve& getCurve() noexcept { return curve; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float shape (float x) const noexcept;

    ShapeCurve curve;
    std::array<float, (size_t) ShapeCurve::tableSize + 1> localTable {};
    int localVersion = -1;

    std::atomic<float>* preParam = nullptr;
    std::atomic<float>* postParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* dcParam = nullptr;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> preGain, postGain;
    juce::SmoothedValue<float> mix;

    float dcCoefficient = 0.997f;
    float xPrevious[2] {};
    float yPrevious[2] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShaperProcessor)
};
