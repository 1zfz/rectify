#pragma once

#include "PluginProcessor.h"

class CurveEditor : public juce::Component, private juce::Timer
{
public:
    explicit CurveEditor (ShapeCurve& curveToEdit);

    std::function<void()> onCurveChanged;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void removePoint (std::vector<CurvePoint>& points, int index);
    int hitTest (const std::vector<CurvePoint>& points, juce::Point<float> position) const;
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toScreen (CurvePoint point) const;
    CurvePoint toCurve (juce::Point<float> position) const;

    ShapeCurve& curve;
    int dragIndex = -1;
    int shownVersion = -1;
};

class ShaperEditor : public juce::AudioProcessorEditor
{
public:
    explicit ShaperEditor (ShaperProcessor& processorToEdit);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    ShaperProcessor& owner;
    CurveEditor curveEditor;
    juce::ComboBox presetBox;
    juce::ToggleButton smoothButton { "Smooth" };
    juce::ToggleButton dcButton { "DC Filter" };
    juce::Slider preSlider, postSlider, mixSlider;
    juce::Label preLabel, postLabel, mixLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment preAttachment, postAttachment, mixAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment dcAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShaperEditor)
};
