#include "ShapeCurve.h"
#include <algorithm>
#include <cmath>

ShapeCurve::ShapeCurve()
{
    points = { { -1.0f, -1.0f }, { 1.0f, 1.0f } };
    rebuild();
}

std::vector<CurvePoint> ShapeCurve::getPoints() const
{
    const juce::SpinLock::ScopedLockType scopedLock (lock);
    return points;
}

void ShapeCurve::setPoints (std::vector<CurvePoint> newPoints)
{
    for (auto& p : newPoints)
    {
        p.x = std::isfinite (p.x) ? juce::jlimit (-1.0f, 1.0f, p.x) : 0.0f;
        p.y = std::isfinite (p.y) ? juce::jlimit (-1.0f, 1.0f, p.y) : 0.0f;
    }

    std::sort (newPoints.begin(), newPoints.end(),
               [] (const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });

    if (newPoints.size() < 2)
        newPoints = { { -1.0f, -1.0f }, { 1.0f, 1.0f } };

    newPoints.front().x = -1.0f;
    newPoints.back().x = 1.0f;

    std::vector<CurvePoint> cleaned { newPoints.front() };

    for (size_t i = 1; i + 1 < newPoints.size() && cleaned.size() < (size_t) maxPoints - 1; ++i)
        if (newPoints[i].x - cleaned.back().x >= 0.002f && 1.0f - newPoints[i].x >= 0.002f)
            cleaned.push_back (newPoints[i]);

    cleaned.push_back (newPoints.back());

    const juce::SpinLock::ScopedLockType scopedLock (lock);
    points = std::move (cleaned);
    rebuild();
}

bool ShapeCurve::isSmooth() const
{
    const juce::SpinLock::ScopedLockType scopedLock (lock);
    return smooth;
}

void ShapeCurve::setSmooth (bool shouldSmooth)
{
    const juce::SpinLock::ScopedLockType scopedLock (lock);
    smooth = shouldSmooth;
    rebuild();
}

juce::StringArray ShapeCurve::getPresetNames()
{
    return juce::StringArray { "Linear",
                               "Full Rectify",
                               "Positive Half Rectify",
                               "Negative Half Rectify",
                               "Soft Clip",
                               "Sine Fold" };
}

void ShapeCurve::applyPreset (int index)
{
    std::vector<CurvePoint> preset;
    bool smoothPreset = false;

    switch (index)
    {
        case 0:
            preset = { { -1.0f, -1.0f }, { 1.0f, 1.0f } };
            break;

        case 1:
            preset = { { -1.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f } };
            break;

        case 2:
            preset = { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f } };
            break;

        case 3:
            preset = { { -1.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } };
            break;

        case 4:
            preset = { { -1.0f, -0.96f }, { -0.5f, -0.76f }, { 0.0f, 0.0f }, { 0.5f, 0.76f }, { 1.0f, 0.96f } };
            smoothPreset = true;
            break;

        case 5:
            for (int i = 0; i <= 16; ++i)
            {
                const float x = -1.0f + (float) i / 8.0f;
                preset.push_back ({ x, std::sin (juce::MathConstants<float>::twoPi * x) });
            }

            smoothPreset = true;
            break;

        default:
            return;
    }

    setSmooth (smoothPreset);
    setPoints (preset);
}

juce::String ShapeCurve::toString() const
{
    const auto current = getPoints();
    juce::StringArray items;

    for (const auto& p : current)
        items.add (juce::String (p.x, 5) + "," + juce::String (p.y, 5));

    return items.joinIntoString (";");
}

void ShapeCurve::fromString (const juce::String& text)
{
    std::vector<CurvePoint> parsed;
    const auto items = juce::StringArray::fromTokens (text, ";", "");

    for (const auto& item : items)
    {
        const auto parts = juce::StringArray::fromTokens (item, ",", "");

        if (parts.size() == 2)
            parsed.push_back ({ parts[0].getFloatValue(), parts[1].getFloatValue() });
    }

    if (parsed.size() >= 2)
        setPoints (std::move (parsed));
}

bool ShapeCurve::copyTable (float* destination, int& copiedVersion) const
{
    const juce::SpinLock::ScopedTryLockType scopedLock (lock);

    if (! scopedLock.isLocked())
        return false;

    std::copy (table.begin(), table.end(), destination);
    copiedVersion = version.load();
    return true;
}

void ShapeCurve::getPreview (float* destination, int count) const
{
    const juce::SpinLock::ScopedLockType scopedLock (lock);

    for (int i = 0; i < count; ++i)
        destination[i] = table[(size_t) ((i * tableSize) / (count - 1))];
}

void ShapeCurve::rebuild()
{
    const int numSegments = (int) points.size() - 1;
    int segment = 0;

    for (int i = 0; i <= tableSize; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / (float) tableSize;

        while (segment < numSegments - 1 && x > points[(size_t) segment + 1].x)
            ++segment;

        table[(size_t) i] = juce::jlimit (-1.0f, 1.0f, evaluate (segment, x));
    }

    version.fetch_add (1);
}

float ShapeCurve::evaluate (int segment, float x) const
{
    const auto& p1 = points[(size_t) segment];
    const auto& p2 = points[(size_t) segment + 1];
    const float h = p2.x - p1.x;
    const float t = juce::jlimit (0.0f, 1.0f, (x - p1.x) / h);

    if (! smooth)
        return p1.y + t * (p2.y - p1.y);

    const auto& p0 = points[(size_t) std::max (segment - 1, 0)];
    const auto& p3 = points[(size_t) std::min (segment + 2, (int) points.size() - 1)];
    const float m1 = (p2.y - p0.y) / (p2.x - p0.x);
    const float m2 = (p3.y - p1.y) / (p3.x - p1.x);
    const float t2 = t * t;
    const float t3 = t2 * t;

    return (2.0f * t3 - 3.0f * t2 + 1.0f) * p1.y
         + (t3 - 2.0f * t2 + t) * h * m1
         + (-2.0f * t3 + 3.0f * t2) * p2.y
         + (t3 - t2) * h * m2;
}
