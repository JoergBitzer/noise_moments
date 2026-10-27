/*
    PdfPlot.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-27
    Description: plot of probability densities over the standardized amplitude (x - mean) / sigma,
    used by both plugins, so their displays can be compared directly:
      - a histogram (bars, AmplitudeHistogram),
      - a Johnson distribution (solid line, the theoretical pdf),
      - the standard normal pdf as reference (dashed line).
    The density axis is linear or logarithmic (the tails, i.e. the kurtosis, are only visible
    in the logarithmic view). Colours come from the LookAndFeel.
    License: MIT
*/
#pragma once

#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>

#include "JohnsonDistribution.h"

namespace jade
{
class PdfPlot : public juce::Component
{
public:
    static constexpr double kRangeInSigma = 6.0; // x axis: -6 ... 6 sigma

    void setDistribution(const JohnsonDistribution& distribution, const juce::String& legendText);
    void clearDistribution();
    void setShowNormal(bool show);
    // densities of equally wide bins from -kRangeInSigma to kRangeInSigma (standardized)
    void setHistogram(const std::vector<float>& densities);
    void clearHistogram();
    void setLogarithmic(bool logarithmic);
    // text in the upper left corner (e.g. "no signal")
    void setMessage(const juce::String& message);

    void paint(juce::Graphics& g) override;

private:
    juce::Rectangle<float> plotArea() const;
    float xToPixel(double x, juce::Rectangle<float> area) const;
    float densityToPixel(double density, juce::Rectangle<float> area) const;
    void updateDensityRange();
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawHistogram(juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawCurve(juce::Graphics& g, juce::Rectangle<float> area, const JohnsonDistribution& d, bool dashed) const;
    void drawLegend(juce::Graphics& g, juce::Rectangle<float> area) const;

    JohnsonDistribution m_distribution;
    bool m_hasDistribution = false;
    juce::String m_legendText;
    bool m_showNormal = true;
    std::vector<float> m_histogram;
    bool m_logarithmic = false;
    juce::String m_message;

    double m_maxDensity = 0.5;    // top of the density axis
    double m_minDensity = 1e-5;   // bottom of the logarithmic axis
};
} // namespace jade
