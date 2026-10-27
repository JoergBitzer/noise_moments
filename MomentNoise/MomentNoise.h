#pragma once

#include <atomic>
#include <random>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

#include "tools/SynchronBlockProcessor.h"
#include "tools/ParameterSpec.h"
#include "PluginSettings.h"
#include "../shared/DistributionDesigner.h"
#include "../shared/PdfPlot.h"

class MomentNoiseAudioProcessor;

// MomentNoise: noise with a chosen skewness and excess kurtosis (Johnson system, see
// shared/JohnsonDistribution.h); the default is Gaussian noise at -24 dBFS (RMS).
// Each parameter is defined here once; tools/ParameterSpec.h makes the parameter
// (jade::makeParameter) and its help text (jade::helpText, e.g. as tooltip) from it.
const struct
{
	const std::string ID = "Skewness";
	const std::string name = "Skewness";
	const std::string unitName = "";
	const float minValue = -2.f;
	const float maxValue = 2.f;
	const float defaultValue = 0.f;
	const int numDecimalPlaces = 2;
	const bool logFrequency = false;
	const std::string help = "Asymmetry of the amplitude distribution (3rd standardized moment): 0 = symmetric, "
	                         "positive = long tail towards positive amplitudes.";
}g_paramSkewness;

const struct
{
	const std::string ID = "ExcessKurtosis";
	const std::string name = "Excess kurtosis";
	const std::string unitName = "";
	const float minValue = -1.5f;
	const float maxValue = 100.f;
	const float defaultValue = 0.f;
	const int numDecimalPlaces = 2;
	const bool logFrequency = false;
	const float knobCentreValue = 3.f;  // exponential knob range (tools/ExponentialRange.h): 3 in the middle
	const std::string help = "Tail weight (4th standardized moment minus 3): 0 = Gaussian, negative = light tails "
	                         "(uniform: -1.2), positive = heavy tails, rare large peaks (Laplace: 3). "
	                         "At least skewness squared - 1.5, smaller values are raised.";
}g_paramExcessKurtosis;

const struct
{
	const std::string ID = "Level";
	const std::string name = "Level";
	const std::string unitName = "dBFS";
	const float minValue = -60.f;
	const float maxValue = 0.f;
	const float defaultValue = -24.f;
	const int numDecimalPlaces = 1;
	const bool logFrequency = false;
	const std::string help = "RMS level of the noise (its standard deviation). Heavy tails give peaks far above "
	                         "the RMS level: they may exceed 0 dBFS.";
}g_paramLevel;

// display setting (an on/off parameter: jade::makeSwitchParameter, see tools/SwitchParameter.h)
const struct
{
	const std::string ID = "LogDensity";
	const std::string name = "Log density";
	const bool defaultIsOn = false;
	const std::string help = "Density axis of the display logarithmic (shows the tails) or linear.";
}g_paramLogDensity;


class MomentNoiseAudio : public SynchronBlockProcessor
{
public:
    MomentNoiseAudio(MomentNoiseAudioProcessor* processor);
    void prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels);
    virtual int processSynchronBlock(juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock);

    // parameter handling
  	void addParameter(std::vector < std::unique_ptr<juce::RangedAudioParameter>>& paramVector);
    void prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>&  vts);

    // some necessary info for the host
    int getLatency(){return m_Latency;};

    // passes the skewness and excess kurtosis parameters to the designer (any thread, realtime safe);
    // the GUI calls it, too: the display follows the parameters also when no audio runs
    void updateDesignTarget() noexcept;
    jade::DistributionDesigner& getDesigner() { return m_designer; }

private:
	MomentNoiseAudioProcessor* m_processor;
    int m_Latency = 0;

    std::atomic<float>* m_skewnessParam = nullptr;
    std::atomic<float>* m_excessKurtosisParam = nullptr;
    std::atomic<float>* m_levelParam = nullptr;

    jade::DistributionDesigner m_designer;
    jade::JohnsonDistribution m_distribution;
    int m_distributionVersion = -1;

    std::mt19937_64 m_randomGenerator;
    std::normal_distribution<double> m_normal;
    juce::SmoothedValue<float> m_gain;
};

class MomentNoiseGUI : public juce::Component, private juce::Timer
{
public:
	MomentNoiseGUI(MomentNoiseAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

	void paint(juce::Graphics& g) override;
	void resized() override;
private:
    void timerCallback() override;
    void updateInfoText(const jade::JohnsonDistribution& d);

	MomentNoiseAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;

    juce::Slider m_skewnessSlider;
    juce::Slider m_excessKurtosisSlider;
    juce::Slider m_levelSlider;
    juce::Label m_skewnessLabel;
    juce::Label m_excessKurtosisLabel;
    juce::Label m_levelLabel;
    juce::ToggleButton m_logDensityButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_skewnessAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_excessKurtosisAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_levelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> m_logDensityAttachment;

    juce::Label m_infoLabel;
    jade::PdfPlot m_plot;
    int m_shownVersion = -1;
};
