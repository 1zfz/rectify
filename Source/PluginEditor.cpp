#include "PluginEditor.h"
#include <cmath>

namespace
{
const juce::Colour accentColour (0xffff6a3d);
constexpr float minGap = 0.01f;

float snap (float v)
{
    const float nearest = std::round (v * 2.0f) * 0.5f;
    return std::abs (v - nearest) < 0.02f ? nearest : v;
}
}

CurveEditor::CurveEditor (ShapeCurve& curveToEdit)
    : curve (curveToEdit)
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    startTimerHz (30);
}

juce::Rectangle<float> CurveEditor::plotArea() const
{
    const auto bounds = getLocalBounds().toFloat().reduced (14.0f);
    const float side = std::min (bounds.getWidth(), bounds.getHeight());
    return bounds.withSizeKeepingCentre (side, side);
}

juce::Point<float> CurveEditor::toScreen (CurvePoint point) const
{
    const auto area = plotArea();
    return { area.getX() + (point.x + 1.0f) * 0.5f * area.getWidth(),
             area.getBottom() - (point.y + 1.0f) * 0.5f * area.getHeight() };
}

CurvePoint CurveEditor::toCurve (juce::Point<float> position) const
{
    const auto area = plotArea();
    return { juce::jlimit (-1.0f, 1.0f, (position.x - area.getX()) / area.getWidth() * 2.0f - 1.0f),
             juce::jlimit (-1.0f, 1.0f, (area.getBottom() - position.y) / area.getHeight() * 2.0f - 1.0f) };
}

int CurveEditor::hitTest (const std::vector<CurvePoint>& points, juce::Point<float> position) const
{
    int best = -1;
    float bestDistance = 16.0f;

    for (size_t i = 0; i < points.size(); ++i)
    {
        const float d = toScreen (points[i]).getDistanceFrom (position);

        if (d <= bestDistance)
        {
            bestDistance = d;
            best = (int) i;
        }
    }

    return best;
}

void CurveEditor::removePoint (std::vector<CurvePoint>& points, int index)
{
    if (index > 0 && index < (int) points.size() - 1)
    {
        points.erase (points.begin() + index);
        curve.setPoints (points);
        repaint();
    }
}

void CurveEditor::timerCallback()
{
    const int current = curve.getVersion();

    if (current != shownVersion)
    {
        shownVersion = current;
        repaint();

        if (onCurveChanged)
            onCurveChanged();
    }
}

void CurveEditor::paint (juce::Graphics& g)
{
    const auto area = plotArea();

    g.setColour (juce::Colour (0xff1a1d22));
    g.fillRoundedRectangle (area, 6.0f);

    for (int i = 0; i <= 4; ++i)
    {
        const float t = (float) i / 4.0f;
        const bool axis = (i == 2);
        const float x = area.getX() + t * area.getWidth();
        const float y = area.getY() + t * area.getHeight();

        g.setColour (juce::Colour (axis ? 0xff3d4550 : 0xff262b32));
        g.drawLine (x, area.getY(), x, area.getBottom(), axis ? 1.5f : 1.0f);
        g.drawLine (area.getX(), y, area.getRight(), y, axis ? 1.5f : 1.0f);
    }

    float preview[257];
    curve.getPreview (preview, 257);

    juce::Path path;

    for (int i = 0; i < 257; ++i)
    {
        const auto p = toScreen ({ -1.0f + 2.0f * (float) i / 256.0f, preview[i] });

        if (i == 0)
            path.startNewSubPath (p);
        else
            path.lineTo (p);
    }

    g.setColour (accentColour);
    g.strokePath (path, juce::PathStrokeType (2.0f));

    const auto points = curve.getPoints();

    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto p = toScreen (points[i]);
        const bool active = (int) i == dragIndex;

        g.setColour (active ? accentColour : juce::Colours::white);
        g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colour (0xff101215));
        g.drawEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f, 1.5f);
    }
}

void CurveEditor::mouseDown (const juce::MouseEvent& e)
{
    auto points = curve.getPoints();
    const int hit = hitTest (points, e.position);

    if (e.mods.isPopupMenu())
    {
        dragIndex = -1;
        removePoint (points, hit);
        return;
    }

    if (hit >= 0)
    {
        dragIndex = hit;
        repaint();
        return;
    }

    if (! plotArea().expanded (8.0f).contains (e.position))
        return;

    if (points.size() >= (size_t) ShapeCurve::maxPoints)
        return;

    auto target = toCurve (e.position);
    target.x = snap (target.x);
    target.y = snap (target.y);

    size_t index = 1;

    while (index < points.size() && points[index].x < target.x)
        ++index;

    if (index >= points.size())
        return;

    if (target.x - points[index - 1].x < minGap || points[index].x - target.x < minGap)
        return;

    points.insert (points.begin() + (std::ptrdiff_t) index, target);
    dragIndex = (int) index;
    curve.setPoints (points);
    repaint();
}

void CurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragIndex < 0)
        return;

    auto points = curve.getPoints();

    if (dragIndex >= (int) points.size())
        return;

    auto target = toCurve (e.position);
    target.x = snap (target.x);
    target.y = snap (target.y);

    const int last = (int) points.size() - 1;

    if (dragIndex == 0)
        target.x = -1.0f;
    else if (dragIndex == last)
        target.x = 1.0f;
    else
        target.x = juce::jmax (points[(size_t) dragIndex - 1].x + minGap,
                               juce::jmin (points[(size_t) dragIndex + 1].x - minGap, target.x));

    points[(size_t) dragIndex] = target;
    curve.setPoints (points);
    repaint();
}

void CurveEditor::mouseUp (const juce::MouseEvent&)
{
    dragIndex = -1;
    repaint();
}

void CurveEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto points = curve.getPoints();
    const int hit = hitTest (points, e.position);
    dragIndex = -1;
    removePoint (points, hit);
}

ShaperEditor::ShaperEditor (ShaperProcessor& processorToEdit)
    : juce::AudioProcessorEditor (processorToEdit),
      owner (processorToEdit),
      curveEditor (processorToEdit.getCurve()),
      preAttachment (processorToEdit.apvts, "pre", preSlider),
      postAttachment (processorToEdit.apvts, "post", postSlider),
      mixAttachment (processorToEdit.apvts, "mix", mixSlider),
      dcAttachment (processorToEdit.apvts, "dc", dcButton)
{
    addAndMakeVisible (curveEditor);

    curveEditor.onCurveChanged = [this]
    {
        smoothButton.setToggleState (owner.getCurve().isSmooth(), juce::dontSendNotification);
    };

    presetBox.addItemList (ShapeCurve::getPresetNames(), 1);
    presetBox.setTextWhenNothingSelected ("Presets");

    presetBox.onChange = [this]
    {
        owner.getCurve().applyPreset (presetBox.getSelectedId() - 1);
        smoothButton.setToggleState (owner.getCurve().isSmooth(), juce::dontSendNotification);
        presetBox.setSelectedId (0, juce::dontSendNotification);
    };

    addAndMakeVisible (presetBox);

    smoothButton.setToggleState (owner.getCurve().isSmooth(), juce::dontSendNotification);
    smoothButton.onClick = [this] { owner.getCurve().setSmooth (smoothButton.getToggleState()); };
    addAndMakeVisible (smoothButton);
    addAndMakeVisible (dcButton);

    auto setupKnob = [this] (juce::Slider& slider, juce::Label& label, const juce::String& name, const juce::String& suffix)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 22);
        slider.setTextValueSuffix (suffix);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
        addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);
    };

    setupKnob (preSlider, preLabel, "Pre", " dB");
    setupKnob (postSlider, postLabel, "Post", " dB");
    setupKnob (mixSlider, mixLabel, "Mix", " %");

    setSize (700, 450);
}

void ShaperEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101215));

    g.setColour (juce::Colours::white);
    g.setFont (20.0f);
    g.drawText ("Rectify Waveshaper", 16, 8, 320, 32, juce::Justification::centredLeft);

    g.setColour (accentColour);
    g.setFont (14.0f);
    g.drawText ("by 1zfz", getWidth() - 176, 8, 160, 32, juce::Justification::centredRight);

    g.setColour (juce::Colour (0xff8b94a1));
    g.setFont (13.0f);
    g.drawFittedText ("Click the plot to add a point.\n"
                      "Drag a point to move it.\n"
                      "Double-click or right-click a point to remove it.\n\n"
                      "Pre pushes the signal into the curve. Raise it a lot to bring up very quiet tails.",
                      414, 312, 272, 124, juce::Justification::topLeft, 8);
}

void ShaperEditor::resized()
{
    curveEditor.setBounds (14, 52, 384, 384);
    presetBox.setBounds (414, 52, 272, 30);
    smoothButton.setBounds (414, 94, 130, 28);
    dcButton.setBounds (556, 94, 130, 28);

    juce::Slider* sliders[] = { &preSlider, &postSlider, &mixSlider };
    juce::Label* labels[] = { &preLabel, &postLabel, &mixLabel };

    for (int i = 0; i < 3; ++i)
    {
        const int x = 414 + i * 92;
        labels[i]->setBounds (x, 138, 88, 20);
        sliders[i]->setBounds (x, 160, 88, 130);
    }
}
