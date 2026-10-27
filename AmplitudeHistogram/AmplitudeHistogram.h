#pragma once

#include <atomic>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

#include "tools/SynchronBlockProcessor.h"
#include "tools/ParameterSpec.h"
#include "PluginSettings.h"
#include "../shared/DistributionDesigner.h"
#include "../shared/PdfPlot.h"

class AmplitudeHistogramAudioProcessor;

// AmplitudeHistogram: histogram of the input amplitudes over the last "Length" seconds (all input
// channels pooled), normalized as a density over the standardized amplitude (x - mean) / sigma, so
// it can be compared with a pdf: the normal pdf and the Johnson pdf with the measured skewness and
// excess kurtosis (the same family MomentNoise uses). The audio passes unchanged.
// Each parameter is defined here once; tools/ParameterSpec.h makes the parameter
// (jade::makeParameter) and its help text (jade::helpText, e.g. as tooltip) from it.
const struct
{
	const std::string ID = "AnalysisLength";
	const std::string name = "Length";
	const std::string unitName = "s";
	const float minValue = 0.1f;
	const float maxValue = 10.f;
	const float defaultValue = 2.f;
	const int numDecimalPlaces = 1;
	const bool logFrequency = false;
	const std::string help = "Analysis length: the histogram is made of the last samples of this duration (all channels). "
	                         "Longer = smoother histogram, but slower to follow changes.";
}g_paramLength;

const struct
{
	const std::string ID = "Bins";
	const std::string name = "Bins";
	const std::string unitName = "";
	const float minValue = 10.f;
	const float maxValue = 400.f;
	const float defaultValue = 120.f;
	const int numDecimalPlaces = 0;
	const bool logFrequency = false;
	const std::string help = "Number of bins from -6 to +6 sigma (120: bin width 0.1 sigma). More bins = finer, "
	                         "but each bin counts fewer samples (noisier).";
}g_paramBins;

const struct
{
	const std::string ID = "Overlay";
	const std::string name = "Overlay";
	const juce::StringArray choices { "None", "Normal", "Johnson fit", "Both" };
	const int defaultIndex = 3;
	const std::string help = "Curves drawn over the histogram: the normal pdf and/or the Johnson pdf with the "
	                         "measured skewness and excess kurtosis.";
}g_paramOverlay;

// display setting (an on/off parameter: jade::makeSwitchParameter, see tools/SwitchParameter.h)
const struct
{
	const std::string ID = "LogDensity";
	const std::string name = "Log density";
	const bool defaultIsOn = false;
	const std::string help = "Density axis logarithmic (shows the tails) or linear.";
}g_paramLogDensity;

// statistics and histogram of the last samples (message thread, no GUI code)
struct HistogramResult
{
    bool valid = false;        // false: no samples or silence
    int numSamples = 0;
    double mean = 0.0;
    double sigma = 0.0;
    double skewness = 0.0;
    double excessKurtosis = 0.0;
    double outsideFraction = 0.0; // part of the samples beyond +-6 sigma (not in the histogram)
    std::vector<float> densities; // bins from -6 to +6 sigma, density of (x - mean) / sigma
};

class HistogramAnalysis
{
public:
    // keeps the samples of the longest analysis length; clears the memory
    void configure(double sampleRate, int numChannels);
    bool isConfiguredFor(double sampleRate, int numChannels) const;
    void push(const float* data, int numSamples);
    HistogramResult analyse(double lengthSeconds, int numBins) const;

private:
    std::vector<float> m_memory; // ring buffer
    int m_writePosition = 0;
    int m_numFilled = 0;
    double m_sampleRate = 0.0;
    int m_numChannels = 0;
};

class AmplitudeHistogramAudio : public SynchronBlockProcessor
{
public:
    AmplitudeHistogramAudio(AmplitudeHistogramAudioProcessor* processor);
    void prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels);
    virtual int processSynchronBlock(juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock);

    // parameter handling
  	void addParameter(std::vector < std::unique_ptr<juce::RangedAudioParameter>>& paramVector);
    void prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>&  vts);

    // some necessary info for the host
    int getLatency(){return m_Latency;};

    // message thread: moves the samples that arrived since the last call into the analysis
    // (the audio thread writes them into a lock-free FIFO; when it is full, new samples are dropped)
    void readNewSamples(HistogramAnalysis& analysis);
    double getSampleRate() const { return m_sampleRate.load(); }
    int getNumChannels() const { return m_numChannels.load(); }

private:
	AmplitudeHistogramAudioProcessor* m_processor;
    int m_Latency = 0;

    juce::CriticalSection m_fifoLock; // prepareToPlay (new size) against readNewSamples, not the audio thread
    juce::AbstractFifo m_fifo { 1 };
    std::vector<float> m_fifoBuffer;
    std::atomic<double> m_sampleRate { 0.0 };
    std::atomic<int> m_numChannels { 0 };
};

class AmplitudeHistogramGUI : public juce::Component, private juce::Timer
{
public:
	AmplitudeHistogramGUI(AmplitudeHistogramAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

	void paint(juce::Graphics& g) override;
	void resized() override;
private:
    void timerCallback() override;
    void analyse();
    void updateOverlay();

	AmplitudeHistogramAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;

    juce::Slider m_lengthSlider;
    juce::Slider m_binsSlider;
    juce::Label m_lengthLabel;
    juce::Label m_binsLabel;
    juce::Label m_overlayLabel;
    juce::ComboBox m_overlayBox;
    juce::ToggleButton m_logDensityButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_lengthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_binsAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> m_overlayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> m_logDensityAttachment;

    juce::Label m_statsLabel;
    jade::PdfPlot m_plot;

    HistogramAnalysis m_analysis;
    HistogramResult m_result;
    jade::DistributionDesigner m_designer; // Johnson fit of the measured moments
    int m_shownDesignVersion = -1;
    int m_ticksSinceAnalysis = 0;
};
