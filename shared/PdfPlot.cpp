/*
    PdfPlot.cpp -- see PdfPlot.h
    License: MIT
*/
#include "PdfPlot.h"

#include <algorithm>
#include <cmath>

namespace jade
{
namespace
{
constexpr float kMarginLeft = 44.0f;
constexpr float kMarginRight = 8.0f;
constexpr float kMarginTop = 8.0f;
constexpr float kMarginBottom = 20.0f;
constexpr float kMinFontSize = 10.0f;
constexpr float kMaxFontSize = 15.0f;
constexpr float kFontSizePerHeight = 1.0f / 18.0f;

constexpr int kPeakSearchPoints = 1201;
constexpr double kHeadroom = 1.08;      // linear axis: 8 % above the highest value
constexpr double kLinearStep = 0.1;     // linear axis: top rounded up to 0.1
constexpr double kLogDecades = 5.0;     // logarithmic axis: 5 decades
constexpr double kNormalPeak = 0.3989422804014327; // 1/sqrt(2 pi)

constexpr float kCurveThickness = 2.0f;
constexpr float kNormalThickness = 1.5f;
constexpr float kGridAlpha = 0.15f;
constexpr float kAxisTextAlpha = 0.7f;
constexpr float kHistogramAlpha = 0.35f;
constexpr float kHistogramOutlineAlpha = 0.6f;
constexpr float kCurveFillAlpha = 0.12f;
constexpr float kDashLength = 6.0f;
constexpr float kLegendLineLength = 22.0f;
constexpr float kLegendRowFactor = 1.4f;     // row height / font size
constexpr float kLegendWidthFactor = 13.0f;  // legend width / font size

float fontSizeFor(float height)
{
    return juce::jlimit(kMinFontSize, kMaxFontSize, height * kFontSizePerHeight);
}

juce::String densityLabel(double value, bool logarithmic)
{
    if (logarithmic)
    {
        const int exponent = static_cast<int>(std::lround(std::log10(value)));
        if (exponent == 0)
        {
            return "1";
        }
        return "1e" + juce::String(exponent);
    }
    return juce::String(value, 2);
}

// a step for the linear density grid that gives about 4 to 8 lines
double linearGridStep(double top)
{
    const double steps[] = { 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0 };
    constexpr double kMaxLines = 8.0;
    for (double step : steps)
    {
        if (top / step <= kMaxLines)
        {
            return step;
        }
    }
    return top / kMaxLines;
}
} // namespace

void PdfPlot::setDistribution(const JohnsonDistribution& distribution, const juce::String& legendText)
{
    m_distribution = distribution;
    m_hasDistribution = true;
    m_legendText = legendText;
    updateDensityRange();
    repaint();
}

void PdfPlot::clearDistribution()
{
    m_hasDistribution = false;
    updateDensityRange();
    repaint();
}

void PdfPlot::setShowNormal(bool show)
{
    m_showNormal = show;
    updateDensityRange();
    repaint();
}

void PdfPlot::setHistogram(const std::vector<float>& densities)
{
    m_histogram = densities;
    updateDensityRange();
    repaint();
}

void PdfPlot::clearHistogram()
{
    m_histogram.clear();
    updateDensityRange();
    repaint();
}

void PdfPlot::setLogarithmic(bool logarithmic)
{
    if (m_logarithmic == logarithmic)
    {
        return;
    }
    m_logarithmic = logarithmic;
    updateDensityRange();
    repaint();
}

void PdfPlot::setMessage(const juce::String& message)
{
    if (m_message == message)
    {
        return;
    }
    m_message = message;
    repaint();
}

void PdfPlot::updateDensityRange()
{
    double highest = 0.0;
    if (m_showNormal)
    {
        highest = kNormalPeak;
    }
    if (m_hasDistribution)
    {
        for (int n = 0; n < kPeakSearchPoints; ++n)
        {
            const double x = -kRangeInSigma + 2.0 * kRangeInSigma * n / (kPeakSearchPoints - 1);
            highest = std::max(highest, m_distribution.pdf(x));
        }
    }
    for (float density : m_histogram)
    {
        highest = std::max(highest, static_cast<double>(density));
    }
    if (highest <= 0.0)
    {
        highest = kNormalPeak;
    }

    if (m_logarithmic)
    {
        m_maxDensity = std::pow(10.0, std::ceil(std::log10(highest)));
        m_minDensity = m_maxDensity * std::pow(10.0, -kLogDecades);
    }
    else
    {
        m_maxDensity = std::ceil(highest * kHeadroom / kLinearStep) * kLinearStep;
        m_minDensity = 0.0;
    }
}

juce::Rectangle<float> PdfPlot::plotArea() const
{
    auto area = getLocalBounds().toFloat();
    area.removeFromLeft(kMarginLeft);
    area.removeFromRight(kMarginRight);
    area.removeFromTop(kMarginTop);
    area.removeFromBottom(kMarginBottom);
    return area;
}

float PdfPlot::xToPixel(double x, juce::Rectangle<float> area) const
{
    const double relative = (x + kRangeInSigma) / (2.0 * kRangeInSigma);
    return area.getX() + static_cast<float>(relative) * area.getWidth();
}

float PdfPlot::densityToPixel(double density, juce::Rectangle<float> area) const
{
    double relative = 0.0;
    if (m_logarithmic)
    {
        const double clamped = std::max(density, m_minDensity);
        relative = (std::log10(clamped) - std::log10(m_minDensity)) / (std::log10(m_maxDensity) - std::log10(m_minDensity));
    }
    else
    {
        relative = density / m_maxDensity;
    }
    relative = juce::jlimit(0.0, 1.0, relative);
    return area.getBottom() - static_cast<float>(relative) * area.getHeight();
}

void PdfPlot::paint(juce::Graphics& g)
{
    const auto background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    g.fillAll(background);
    const auto area = plotArea();
    drawGrid(g, area);
    drawHistogram(g, area);
    if (m_showNormal)
    {
        drawCurve(g, area, JohnsonDistribution(), true);
    }
    if (m_hasDistribution)
    {
        drawCurve(g, area, m_distribution, false);
    }
    drawLegend(g, area);

    if (m_message.isNotEmpty())
    {
        g.setColour(getLookAndFeel().findColour(juce::Label::textColourId));
        g.setFont(fontSizeFor(static_cast<float>(getHeight())));
        g.drawText(m_message, area.reduced(6.0f), juce::Justification::topLeft);
    }
}

void PdfPlot::drawGrid(juce::Graphics& g, juce::Rectangle<float> area) const
{
    const auto text = getLookAndFeel().findColour(juce::Label::textColourId);
    const float fontSize = fontSizeFor(static_cast<float>(getHeight()));
    g.setFont(fontSize);

    // x: every sigma
    const int range = static_cast<int>(kRangeInSigma);
    for (int k = -range; k <= range; ++k)
    {
        const float px = xToPixel(k, area);
        g.setColour(text.withAlpha(kGridAlpha));
        g.drawVerticalLine(juce::roundToInt(px), area.getY(), area.getBottom());
        g.setColour(text.withAlpha(kAxisTextAlpha));
        const juce::Rectangle<float> label(px - fontSize * 1.5f, area.getBottom() + 2.0f, fontSize * 3.0f, fontSize);
        g.drawText(juce::String(k), label, juce::Justification::centred);
    }

    // density
    std::vector<double> lines;
    if (m_logarithmic)
    {
        for (double d = m_maxDensity; d >= m_minDensity * 0.999; d *= 0.1)
        {
            lines.push_back(d);
        }
    }
    else
    {
        const double step = linearGridStep(m_maxDensity);
        for (double d = 0.0; d <= m_maxDensity * 1.0001; d += step)
        {
            lines.push_back(d);
        }
    }
    for (double d : lines)
    {
        const float py = densityToPixel(d, area);
        g.setColour(text.withAlpha(kGridAlpha));
        g.drawHorizontalLine(juce::roundToInt(py), area.getX(), area.getRight());
        g.setColour(text.withAlpha(kAxisTextAlpha));
        const juce::Rectangle<float> label(0.0f, py - fontSize * 0.5f, kMarginLeft - 4.0f, fontSize);
        g.drawText(densityLabel(d, m_logarithmic), label, juce::Justification::centredRight);
    }

    g.setColour(text.withAlpha(kAxisTextAlpha));
    g.drawText("x axis: (x - mean) / sigma", area.reduced(6.0f, 4.0f), juce::Justification::bottomLeft);
    g.drawRect(area);
}

void PdfPlot::drawHistogram(juce::Graphics& g, juce::Rectangle<float> area) const
{
    if (m_histogram.empty())
    {
        return;
    }
    const auto colour = getLookAndFeel().findColour(juce::Label::textColourId);
    const double binWidth = 2.0 * kRangeInSigma / static_cast<double>(m_histogram.size());
    juce::Path outline;
    juce::RectangleList<float> bars;
    const float bottom = area.getBottom();
    for (size_t n = 0; n < m_histogram.size(); ++n)
    {
        const double left = -kRangeInSigma + binWidth * static_cast<double>(n);
        const float x0 = xToPixel(left, area);
        const float x1 = xToPixel(left + binWidth, area);
        const float top = densityToPixel(m_histogram[n], area);
        bars.addWithoutMerging({ x0, top, x1 - x0, bottom - top });
        if (n == 0)
        {
            outline.startNewSubPath(x0, bottom);
        }
        outline.lineTo(x0, top);
        outline.lineTo(x1, top);
    }
    outline.lineTo(area.getRight(), bottom);
    g.setColour(colour.withAlpha(kHistogramAlpha));
    g.fillRectList(bars);
    g.setColour(colour.withAlpha(kHistogramOutlineAlpha));
    g.strokePath(outline, juce::PathStrokeType(1.0f));
}

void PdfPlot::drawCurve(juce::Graphics& g, juce::Rectangle<float> area, const JohnsonDistribution& d, bool dashed) const
{
    const int numPoints = std::max(2, static_cast<int>(area.getWidth()));
    juce::Path curve;
    for (int n = 0; n < numPoints; ++n)
    {
        const double x = -kRangeInSigma + 2.0 * kRangeInSigma * n / (numPoints - 1);
        const float px = xToPixel(x, area);
        const float py = densityToPixel(d.pdf(x), area);
        if (n == 0)
        {
            curve.startNewSubPath(px, py);
        }
        else
        {
            curve.lineTo(px, py);
        }
    }

    if (dashed)
    {
        g.setColour(getLookAndFeel().findColour(juce::Label::textColourId).withAlpha(kAxisTextAlpha));
        juce::Path dashedCurve;
        const float dashes[] = { kDashLength, kDashLength };
        juce::PathStrokeType(kNormalThickness).createDashedStroke(dashedCurve, curve, dashes, 2);
        g.fillPath(dashedCurve);
        return;
    }

    const auto colour = getLookAndFeel().findColour(juce::Slider::rotarySliderFillColourId);
    if (m_histogram.empty())
    {
        juce::Path filled(curve);
        filled.lineTo(area.getRight(), area.getBottom());
        filled.lineTo(area.getX(), area.getBottom());
        filled.closeSubPath();
        g.setColour(colour.withAlpha(kCurveFillAlpha));
        g.fillPath(filled);
    }
    g.setColour(colour);
    g.strokePath(curve, juce::PathStrokeType(kCurveThickness));
}

void PdfPlot::drawLegend(juce::Graphics& g, juce::Rectangle<float> area) const
{
    const float fontSize = fontSizeFor(static_cast<float>(getHeight()));
    const float rowHeight = fontSize * kLegendRowFactor;
    const float width = fontSize * kLegendWidthFactor;
    const auto text = getLookAndFeel().findColour(juce::Label::textColourId);
    float y = area.getY() + 4.0f;
    const float x = area.getRight() - width;
    g.setFont(fontSize);

    if (!m_histogram.empty())
    {
        g.setColour(text.withAlpha(kHistogramAlpha));
        g.fillRect(x, y + rowHeight * 0.25f, kLegendLineLength, rowHeight * 0.5f);
        g.setColour(text);
        g.drawText("histogram", juce::Rectangle<float>(x + kLegendLineLength + 6.0f, y, width, rowHeight),
                   juce::Justification::centredLeft);
        y += rowHeight;
    }
    if (m_hasDistribution)
    {
        g.setColour(getLookAndFeel().findColour(juce::Slider::rotarySliderFillColourId));
        g.fillRect(x, y + rowHeight * 0.5f - 1.0f, kLegendLineLength, kCurveThickness);
        g.setColour(text);
        g.drawText(m_legendText, juce::Rectangle<float>(x + kLegendLineLength + 6.0f, y, width, rowHeight),
                   juce::Justification::centredLeft);
        y += rowHeight;
    }
    if (m_showNormal)
    {
        g.setColour(text.withAlpha(kAxisTextAlpha));
        g.fillRect(x, y + rowHeight * 0.5f - 0.75f, kDashLength, kNormalThickness);
        g.fillRect(x + 2.0f * kDashLength, y + rowHeight * 0.5f - 0.75f, kDashLength, kNormalThickness);
        g.setColour(text);
        g.drawText("normal", juce::Rectangle<float>(x + kLegendLineLength + 6.0f, y, width, rowHeight),
                   juce::Justification::centredLeft);
    }
}
} // namespace jade
