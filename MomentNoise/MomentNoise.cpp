#include <math.h>
#include "MomentNoise.h"

#include "PluginProcessor.h"
#include "tools/SwitchParameter.h"
#include "tools/ExponentialRange.h"

namespace
{
constexpr double kLevelSmoothingSeconds = 0.02;
constexpr int kGuiUpdatesPerSecond = 30;
constexpr float kSwitchOnThreshold = 0.5f;
constexpr float kInfoFontSize = 14.0f;
constexpr float kLabelFontSize = 15.0f;
constexpr int kTextBoxWidth = 80;
constexpr int kTextBoxHeight = 20;
}

MomentNoiseAudio::MomentNoiseAudio(MomentNoiseAudioProcessor* processor)
:SynchronBlockProcessor(), m_processor(processor), m_randomGenerator(std::random_device{}())
{
}

void MomentNoiseAudio::prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels)
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

    // the first block must already have the right distribution: design it here (not realtime)
    updateDesignTarget();
    m_designer.designNow();
    m_distribution = m_designer.getDesign();
    m_distributionVersion = m_designer.getVersion();

    m_gain.reset(sampleRate, kLevelSmoothingSeconds);
    m_gain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(m_levelParam->load()));
}

void MomentNoiseAudio::updateDesignTarget() noexcept
{
    if (m_skewnessParam == nullptr)
    {
        return;
    }
    m_designer.setTarget(m_skewnessParam->load(), m_excessKurtosisParam->load());
}

int MomentNoiseAudio::processSynchronBlock(juce::AudioBuffer<float> & buffer, juce::MidiBuffer &midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(midiMessages, NrOfBlocksSinceLastProcessBlock);

    updateDesignTarget();
    if (m_designer.getVersion() != m_distributionVersion)
    {
        // if the designer is just publishing, the next block takes the new design
        m_designer.tryGetDesign(m_distribution, m_distributionVersion);
    }
    m_gain.setTargetValue(juce::Decibels::decibelsToGain(m_levelParam->load()));

    // generator: the input is replaced; independent noise in every channel
    const int numChannels = buffer.getNumChannels();
    auto channelData = buffer.getArrayOfWritePointers();
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        const float gain = m_gain.getNextValue();
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const double x = m_distribution.transform(m_normal(m_randomGenerator));
            channelData[channel][n] = gain * static_cast<float>(x);
        }
    }
    return 0;
}

void MomentNoiseAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>> &paramVector)
{
    // one line per parameter, made from its definition in MomentNoise.h (see tools/ParameterSpec.h)
    paramVector.push_back(jade::makeParameter(g_paramSkewness));
    paramVector.push_back(jade::makeParameter(g_paramExcessKurtosis,
        jade::makeExponentialRange(g_paramExcessKurtosis.minValue, g_paramExcessKurtosis.maxValue,
                                   g_paramExcessKurtosis.knobCentreValue, g_paramExcessKurtosis.numDecimalPlaces)));
    paramVector.push_back(jade::makeParameter(g_paramLevel));
    paramVector.push_back(jade::makeSwitchParameter(g_paramLogDensity.ID, g_paramLogDensity.name, g_paramLogDensity.defaultIsOn));
    jade::parameterHelpTexts()[juce::String(g_paramLogDensity.ID)] = juce::String(g_paramLogDensity.help);
}

void MomentNoiseAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState> &vts)
{
    m_skewnessParam = vts->getRawParameterValue(g_paramSkewness.ID);
    m_excessKurtosisParam = vts->getRawParameterValue(g_paramExcessKurtosis.ID);
    m_levelParam = vts->getRawParameterValue(g_paramLevel.ID);
    updateDesignTarget();
}


MomentNoiseGUI::MomentNoiseGUI(MomentNoiseAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
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
        { m_skewnessSlider, m_skewnessLabel, g_paramSkewness.ID, g_paramSkewness.name, jade::helpText(g_paramSkewness), m_skewnessAttachment },
        { m_excessKurtosisSlider, m_excessKurtosisLabel, g_paramExcessKurtosis.ID, g_paramExcessKurtosis.name,
          jade::helpText(g_paramExcessKurtosis), m_excessKurtosisAttachment },
        { m_levelSlider, m_levelLabel, g_paramLevel.ID, g_paramLevel.name, jade::helpText(g_paramLevel), m_levelAttachment },
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

    m_logDensityButton.setButtonText(g_paramLogDensity.name);
    m_logDensityButton.setTooltip(juce::String(g_paramLogDensity.help));
    m_logDensityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        m_apvts, g_paramLogDensity.ID, m_logDensityButton);
    addAndMakeVisible(m_logDensityButton);

    m_infoLabel.setFont(juce::FontOptions(kInfoFontSize));
    m_infoLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(m_infoLabel);

    addAndMakeVisible(m_plot);
    timerCallback(); // show the current design at once
    startTimerHz(kGuiUpdatesPerSecond);
}

void MomentNoiseGUI::timerCallback()
{
    auto& algo = m_processor.m_algo;
    algo.updateDesignTarget(); // the display follows the parameters also when no audio runs
    m_plot.setLogarithmic(m_apvts.getRawParameterValue(g_paramLogDensity.ID)->load() >= kSwitchOnThreshold);

    const int version = algo.getDesigner().getVersion();
    if (version == m_shownVersion)
    {
        return;
    }
    m_shownVersion = version;
    const jade::JohnsonDistribution d = algo.getDesigner().getDesign();
    m_plot.setDistribution(d, "theoretical pdf");
    updateInfoText(d);
}

void MomentNoiseGUI::updateInfoText(const jade::JohnsonDistribution& d)
{
    juce::String text;
    text << "Johnson family: " << jade::familyName(d.family) << "\n";
    text << "skewness " << juce::String(d.skewness, 2) << "\n";
    text << "excess kurtosis " << juce::String(d.excessKurtosis, 2);
    if (d.limited)
    {
        text << "\nexcess kurtosis raised to the smallest possible value";
    }
    m_infoLabel.setText(text, juce::dontSendNotification);
}

void MomentNoiseGUI::paint(juce::Graphics &g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).brighter(0.3f));

    // Take colours from the LookAndFeel (findColour), not fixed ones like juce::Colour(0xffffffff):
    // then your GUI follows the day/night theme (WITH_DAYNIGHT) and any other LookAndFeel.
    g.setColour (getLookAndFeel().findColour (juce::Label::textColourId));
    g.setFont (12.0f);

    juce::String text2display = "MomentNoise V " + juce::String(PLUGIN_VERSION_MAJOR) + "." + juce::String(PLUGIN_VERSION_MINOR) + "." + juce::String(PLUGIN_VERSION_PATCH);
    g.drawFittedText (text2display, getLocalBounds().reduced(4, 0), juce::Justification::topRight, 1);

}

void MomentNoiseGUI::resized()
{
    constexpr float kControlsShare = 0.5f;      // upper half: controls, lower half: display
    constexpr float kKnobsWidthShare = 0.6f;    // knobs on the left, info on the right
    constexpr float kLabelHeightShare = 0.14f;  // of the control area
    constexpr int kMargin = 6;
    constexpr int kButtonHeight = 24;

	auto r = getLocalBounds();
    auto controls = r.removeFromTop(static_cast<int>(r.getHeight() * kControlsShare));
    m_plot.setBounds(r.reduced(kMargin));

    controls.reduce(kMargin, kMargin);
    auto knobs = controls.removeFromLeft(static_cast<int>(controls.getWidth() * kKnobsWidthShare));
    const int labelHeight = static_cast<int>(knobs.getHeight() * kLabelHeightShare);
    const int knobWidth = knobs.getWidth() / 3;
    juce::Slider* sliders[] = { &m_skewnessSlider, &m_excessKurtosisSlider, &m_levelSlider };
    juce::Label* labels[] = { &m_skewnessLabel, &m_excessKurtosisLabel, &m_levelLabel };
    for (int k = 0; k < 3; ++k)
    {
        auto column = knobs.removeFromLeft(knobWidth);
        labels[k]->setBounds(column.removeFromTop(labelHeight));
        sliders[k]->setBounds(column);
    }

    controls.removeFromLeft(kMargin);
    controls.removeFromTop(labelHeight); // below the version text
    m_logDensityButton.setBounds(controls.removeFromBottom(kButtonHeight));
    m_infoLabel.setBounds(controls);
}
