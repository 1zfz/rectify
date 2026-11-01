#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <vector>

struct CurvePoint
{
    float x;
    float y;
};

class ShapeCurve
{
public:
    static constexpr int tableSize = 8192;
    static constexpr int maxPoints = 32;

    ShapeCurve();

    std::vector<CurvePoint> getPoints() const;
    void setPoints (std::vector<CurvePoint> newPoints);

    bool isSmooth() const;
    void setSmooth (bool shouldSmooth);

    void applyPreset (int index);
    static juce::StringArray getPresetNames();

    juce::String toString() const;
    void fromString (const juce::String& text);

    bool copyTable (float* destination, int& copiedVersion) const;
    void getPreview (float* destination, int count) const;

    int getVersion() const noexcept { return version.load(); }

private:
    void rebuild();
    float evaluate (int segment, float x) const;

    mutable juce::SpinLock lock;
    std::vector<CurvePoint> points;
    std::array<float, (size_t) tableSize + 1> table {};
    bool smooth = false;
    std::atomic<int> version { 0 };
};
