#include <math.h>
#include <algorithm>
#include "AmplitudeHistogram.h"

#include "PluginProcessor.h"
#include "tools/SwitchParameter.h"

namespace
{
constexpr double kFifoSeconds = 1.0;       // the GUI empties the FIFO 30 times per second
constexpr double kSilenceSigma = 1e-7;     // -140 dBFS: below this, the input counts as silence
constexpr int kGuiUpdatesPerSecond = 30;
constexpr int kTicksPerAnalysis = 3;       // histogram 10 times per second
constexpr double kFitRounding = 100.0;     // the Johnson fit uses the moments rounded to 0.01
constexpr float kSwitchOnThreshold = 0.5f;
constexpr float kStatsFontSize = 14.0f;
constexpr float kLabelFontSize = 15.0f;
constexpr int kTextBoxWidth = 70;
constexpr int kTextBoxHeight = 20;

enum OverlayChoice
{
    kOverlayNone = 0,
    kOverlayNormal,
    kOverlayJohnson,
    kOverlayBoth
};

double roundForFit(double value)
{
    return std::round(value * kFitRounding) / kFitRounding;
}
}

// ---------------------------------------------------------------- HistogramAnalysis
void HistogramAnalysis::configure(double sampleRate, int numChannels)
{
    m_sampleRate = sampleRate;
    m_numChannels = numChannels;
    const auto size = static_cast<size_t>(std::ceil(g_paramLength.maxValue * sampleRate * numChannels));
    m_memory.assign(std::max<size_t>(size, 1), 0.0f);
    m_writePosition = 0;
    m_numFilled = 0;
}

bool HistogramAnalysis::isConfiguredFor(double sampleRate, int numChannels) const
{
    return juce::exactlyEqual(m_sampleRate, sampleRate) && m_numChannels == numChannels;
}

void HistogramAnalysis::push(const float* data, int numSamples)
{
    const int size = static_cast<int>(m_memory.size());
    for (int n = 0; n < numSamples; ++n)
    {
        m_memory[static_cast<size_t>(m_writePosition)] = data[n];
        m_writePosition = (m_writePosition + 1) % size;
    }
    m_numFilled = std::min(size, m_numFilled + numSamples);
}

HistogramResult HistogramAnalysis::analyse(double lengthSeconds, int numBins) const
{
    HistogramResult result;
    const int size = static_cast<int>(m_memory.size());
    const int wanted = static_cast<int>(std::round(lengthSeconds * m_sampleRate * m_numChannels));
    const int count = std::min(m_numFilled, wanted);
    if (count < 2)
    {
        return result;
    }
    // the last "count" samples of the ring buffer
    const int start = (m_writePosition - count + size) % size;
    auto sampleAt = [this, start, size](int n)
    {
        return static_cast<double>(m_memory[static_cast<size_t>((start + n) % size)]);
    };

    double sum = 0.0;
    for (int n = 0; n < count; ++n)
    {
        sum += sampleAt(n);
    }
    const double mean = sum / count;

    double sum2 = 0.0;
    double sum3 = 0.0;
    double sum4 = 0.0;
    for (int n = 0; n < count; ++n)
    {
        const double d = sampleAt(n) - mean;
        const double d2 = d * d;
        sum2 += d2;
        sum3 += d2 * d;
        sum4 += d2 * d2;
    }
    const double variance = sum2 / count;
    result.numSamples = count;
    result.mean = mean;
    result.sigma = std::sqrt(variance);
    if (result.sigma < kSilenceSigma)
    {
        return result;
    }
    result.skewness = (sum3 / count) / (variance * result.sigma);
    result.excessKurtosis = (sum4 / count) / (variance * variance) - 3.0;

    // histogram of (x - mean) / sigma from -range to +range
    const double range = jade::PdfPlot::kRangeInSigma;
    const double binWidth = 2.0 * range / numBins;
    std::vector<int> counts(static_cast<size_t>(numBins), 0);
    int outside = 0;
    for (int n = 0; n < count; ++n)
    {
        const double u = (sampleAt(n) - mean) / result.sigma;
        const int bin = static_cast<int>(std::floor((u + range) / binWidth));
        if (bin < 0 || bin >= numBins)
        {
            ++outside;
        }
        else
        {
            ++counts[static_cast<size_t>(bin)];
        }
    }
    result.densities.resize(counts.size());
    for (size_t b = 0; b < counts.size(); ++b)
    {
        result.densities[b] = static_cast<float>(counts[b] / (count * binWidth));
    }
    result.outsideFraction = static_cast<double>(outside) / count;
    result.valid = true;
    return result;
}

// ---------------------------------------------------------------- AmplitudeHistogramAudio
AmplitudeHistogramAudio::AmplitudeHistogramAudio(AmplitudeHistogramAudioProcessor* processor)
:SynchronBlockProcessor(), m_processor(processor)
{
}

void AmplitudeHistogramAudio::prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels)
{
    juce::ignoreUnused(max_samplesPerBlock,max_channels);
    int synchronblocksize;
    synchronblocksize = static_cast<int>(round(g_desired_blocksize_ms * sampleRate * 0.001)); // 0.001 to transform ms to seconds;
    if (g_forcePowerOf2)
    {
        int nextpowerof2 = int(log2(synchronblocksize))+1;
        synchronblocksize = int(pow(2,nextpowerof2));
    }
    prepareSynchronProcessing(max_channels,synchronblocksize);
    m_Latency = synchronblocksize; // '=', not '+=': prepareToPlay can be called several times

    const juce::ScopedLock lock(m_fifoLock);
    const int fifoSize = static_cast<int>(std::ceil(kFifoSeconds * sampleRate * max_channels));
    m_fifoBuffer.assign(static_cast<size_t>(fifoSize), 0.0f);
    m_fifo.setTotalSize(fifoSize);
    m_fifo.reset();
    m_sampleRate.store(sampleRate);
    m_numChannels.store(max_channels);
}

int AmplitudeHistogramAudio::processSynchronBlock(juce::AudioBuffer<float> & buffer, juce::MidiBuffer &midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(midiMessages, NrOfBlocksSinceLastProcessBlock);
    // analyzer: the audio passes unchanged; all channels go into the FIFO (pooled)
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const float* data = buffer.getReadPointer(channel);
        const auto scope = m_fifo.write(buffer.getNumSamples()); // writes less if the FIFO is full
        if (scope.blockSize1 > 0)
        {
            std::copy(data, data + scope.blockSize1, m_fifoBuffer.begin() + scope.startIndex1);
        }
        if (scope.blockSize2 > 0)
        {
            std::copy(data + scope.blockSize1, data + scope.blockSize1 + scope.blockSize2,
                      m_fifoBuffer.begin() + scope.startIndex2);
        }
    }
    return 0;
}

void AmplitudeHistogramAudio::readNewSamples(HistogramAnalysis& analysis)
{
    const juce::ScopedLock lock(m_fifoLock);
    const auto scope = m_fifo.read(m_fifo.getNumReady());
    if (scope.blockSize1 > 0)
    {
        analysis.push(m_fifoBuffer.data() + scope.startIndex1, scope.blockSize1);
    }
    if (scope.blockSize2 > 0)
    {
        analysis.push(m_fifoBuffer.data() + scope.startIndex2, scope.blockSize2);
    }
}

void AmplitudeHistogramAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>> &paramVector)
{
    // one line per parameter, made from its definition in AmplitudeHistogram.h (see tools/ParameterSpec.h)
    paramVector.push_back(jade::makeParameter(g_paramLength));
    paramVector.push_back(jade::makeParameter(g_paramBins));
    const int kParameterVersion = 1;
    paramVector.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(g_paramOverlay.ID, kParameterVersion),
                                                                       g_paramOverlay.name, g_paramOverlay.choices,
                                                                       g_paramOverlay.defaultIndex));
    jade::parameterHelpTexts()[juce::String(g_paramOverlay.ID)] = juce::String(g_paramOverlay.help);
    paramVector.push_back(jade::makeSwitchParameter(g_paramLogDensity.ID, g_paramLogDensity.name, g_paramLogDensity.defaultIsOn));
    jade::parameterHelpTexts()[juce::String(g_paramLogDensity.ID)] = juce::String(g_paramLogDensity.help);
}

void AmplitudeHistogramAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState> &vts)
{
    juce::ignoreUnused(vts);
}

// ---------------------------------------------------------------- AmplitudeHistogramGUI
AmplitudeHistogramGUI::AmplitudeHistogramGUI(AmplitudeHistogramAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
:m_processor(p) ,m_apvts(apvts)
{
    struct Knob
    {
        juce::Slider& slider;
        juce::Label& label;
        const std::string& id;
        const std::string& name;
        juce::String tooltip;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment;
    };
    Knob knobs[] = {
        { m_lengthSlider, m_lengthLabel, g_paramLength.ID, g_paramLength.name, jade::helpText(g_paramLength), m_lengthAttachment },
        { m_binsSlider, m_binsLabel, g_paramBins.ID, g_paramBins.name, jade::helpText(g_paramBins), m_binsAttachment },
    };
    for (auto& knob : knobs)
    {
        knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, kTextBoxWidth, kTextBoxHeight);
        knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(m_apvts, knob.id, knob.slider);
        knob.slider.setTooltip(knob.tooltip);
        addAndMakeVisible(knob.slider);
        knob.label.setText(knob.name, juce::dontSendNotification);
        knob.label.setJustificationType(juce::Justification::centred);
        knob.label.setFont(juce::FontOptions(kLabelFontSize));
        addAndMakeVisible(knob.label);
    }

    m_overlayLabel.setText(g_paramOverlay.name, juce::dontSendNotification);
    m_overlayLabel.setFont(juce::FontOptions(kLabelFontSize));
    addAndMakeVisible(m_overlayLabel);
    m_overlayBox.addItemList(g_paramOverlay.choices, 1);
    m_overlayBox.setTooltip(juce::String(g_paramOverlay.help));
    m_overlayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        m_apvts, g_paramOverlay.ID, m_overlayBox);
    addAndMakeVisible(m_overlayBox);

    m_logDensityButton.setButtonText(g_paramLogDensity.name);
    m_logDensityButton.setTooltip(juce::String(g_paramLogDensity.help));
    m_logDensityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        m_apvts, g_paramLogDensity.ID, m_logDensityButton);
    addAndMakeVisible(m_logDensityButton);

    m_statsLabel.setFont(juce::FontOptions(kStatsFontSize));
    m_statsLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(m_statsLabel);

    addAndMakeVisible(m_plot);
    analyse();
    startTimerHz(kGuiUpdatesPerSecond);
}

void AmplitudeHistogramGUI::timerCallback()
{
    auto& algo = m_processor.m_algo;
    const double sampleRate = algo.getSampleRate();
    const int numChannels = algo.getNumChannels();
    if (sampleRate > 0.0 && !m_analysis.isConfiguredFor(sampleRate, numChannels))
    {
        m_analysis.configure(sampleRate, numChannels);
    }
    if (sampleRate > 0.0)
    {
        algo.readNewSamples(m_analysis); // every tick: the FIFO must not run full
    }

    ++m_ticksSinceAnalysis;
    if (m_ticksSinceAnalysis >= kTicksPerAnalysis)
    {
        m_ticksSinceAnalysis = 0;
        analyse();
    }
    m_plot.setLogarithmic(m_apvts.getRawParameterValue(g_paramLogDensity.ID)->load() >= kSwitchOnThreshold);
    updateOverlay();
}

void AmplitudeHistogramGUI::analyse()
{
    const double length = m_apvts.getRawParameterValue(g_paramLength.ID)->load();
    const int bins = static_cast<int>(m_apvts.getRawParameterValue(g_paramBins.ID)->load());
    m_result = m_analysis.analyse(length, bins);

    if (!m_result.valid)
    {
        m_plot.clearHistogram();
        m_plot.setMessage("no input signal");
        m_statsLabel.setText("", juce::dontSendNotification);
        return;
    }
    m_plot.setMessage("");
    m_plot.setHistogram(m_result.densities);
    m_designer.setTarget(static_cast<float>(roundForFit(m_result.skewness)), static_cast<float>(roundForFit(m_result.excessKurtosis)));

    juce::String text;
    text << "N = " << m_result.numSamples << " samples\n";
    text << "mean " << juce::String(m_result.mean, 5) << ",  RMS (sigma) "
         << juce::String(juce::Decibels::gainToDecibels(m_result.sigma), 1) << " dBFS\n";
    text << "skewness " << juce::String(m_result.skewness, 3) << "\n";
    text << "excess kurtosis " << juce::String(m_result.excessKurtosis, 3) << "\n";
    text << "beyond +-6 sigma: " << juce::String(100.0 * m_result.outsideFraction, 3) << " %";
    m_statsLabel.setText(text, juce::dontSendNotification);
}

void AmplitudeHistogramGUI::updateOverlay()
{
    const int overlay = static_cast<int>(m_apvts.getRawParameterValue(g_paramOverlay.ID)->load());
    const bool showNormal = overlay == kOverlayNormal || overlay == kOverlayBoth;
    const bool showJohnson = (overlay == kOverlayJohnson || overlay == kOverlayBoth) && m_result.valid;
    m_plot.setShowNormal(showNormal);
    if (!showJohnson)
    {
        m_plot.clearDistribution();
        m_shownDesignVersion = -1;
        return;
    }
    const int version = m_designer.getVersion();
    if (version == m_shownDesignVersion || version == 0)
    {
        return;
    }
    m_shownDesignVersion = version;
    const jade::JohnsonDistribution d = m_designer.getDesign();
    juce::String legend = "Johnson fit";
    if (d.limited)
    {
        legend << " (limited)";
    }
    m_plot.setDistribution(d, legend);
}

void AmplitudeHistogramGUI::paint(juce::Graphics &g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).brighter(0.3f));

    // Take colours from the LookAndFeel (findColour), not fixed ones like juce::Colour(0xffffffff):
    // then your GUI follows the day/night theme (WITH_DAYNIGHT) and any other LookAndFeel.
    g.setColour (getLookAndFeel().findColour (juce::Label::textColourId));
    g.setFont (12.0f);

    juce::String text2display = "AmplitudeHistogram V " + juce::String(PLUGIN_VERSION_MAJOR) + "." + juce::String(PLUGIN_VERSION_MINOR) + "." + juce::String(PLUGIN_VERSION_PATCH);
    g.drawFittedText (text2display, getLocalBounds().reduced(4, 0), juce::Justification::topRight, 1);

}

void AmplitudeHistogramGUI::resized()
{
    constexpr float kControlsShare = 0.36f;    // upper part: controls and statistics, lower part: display
    constexpr float kKnobsWidthShare = 0.32f;
    constexpr float kChoiceWidthShare = 0.3f;  // overlay and log switch (of the rest)
    constexpr float kLabelHeightShare = 0.16f;
    constexpr int kMargin = 6;
    constexpr int kRowHeight = 24;

	auto r = getLocalBounds();
    auto controls = r.removeFromTop(static_cast<int>(r.getHeight() * kControlsShare));
    m_plot.setBounds(r.reduced(kMargin));

    controls.reduce(kMargin, kMargin);
    auto knobs = controls.removeFromLeft(static_cast<int>(controls.getWidth() * kKnobsWidthShare));
    const int labelHeight = static_cast<int>(knobs.getHeight() * kLabelHeightShare);
    const int knobWidth = knobs.getWidth() / 2;
    auto lengthColumn = knobs.removeFromLeft(knobWidth);
    m_lengthLabel.setBounds(lengthColumn.removeFromTop(labelHeight));
    m_lengthSlider.setBounds(lengthColumn);
    m_binsLabel.setBounds(knobs.removeFromTop(labelHeight));
    m_binsSlider.setBounds(knobs);

    controls.removeFromLeft(kMargin);
    auto choices = controls.removeFromLeft(static_cast<int>(controls.getWidth() * kChoiceWidthShare));
    m_overlayLabel.setBounds(choices.removeFromTop(labelHeight));
    m_overlayBox.setBounds(choices.removeFromTop(kRowHeight));
    choices.removeFromTop(kMargin);
    m_logDensityButton.setBounds(choices.removeFromTop(kRowHeight));

    controls.removeFromLeft(kMargin);
    controls.removeFromTop(labelHeight); // below the version text
    m_statsLabel.setBounds(controls);
}
