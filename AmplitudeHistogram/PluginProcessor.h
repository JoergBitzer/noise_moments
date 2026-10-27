#pragma once
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "tools/MidiModPitchState.h"
#include "tools/PresetHandler.h"
#include "tools/BypassDelay.h"
#include "AmplitudeHistogram.h"

//==============================================================================
class AmplitudeHistogramAudioProcessor  : public juce::AudioProcessor
{
public:
    friend class AmplitudeHistogramAudioProcessorEditor;
    //==============================================================================
    AmplitudeHistogramAudioProcessor();
    ~AmplitudeHistogramAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    // the plugin reports a latency (synchronous blocks): bypassed, an effect passes its input through
    // delayed by the same amount, a synth is silent (JUCE's default would assert)
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlockBypassed;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    float getScaleFactor(){return m_pluginScaleFactor;};
    void setScaleFactor(float newscalefactor){m_pluginScaleFactor = newscalefactor;};
    
    // Algo component and ValueTreeState must be public to be accessed by the editor
    AmplitudeHistogramAudio m_algo;
    std::unique_ptr<AudioProcessorValueTreeState> m_parameterVTS;
private:
    CriticalSection m_protect;
    float m_fs; // sampling rate is always needed

    //Parameterhandling
    std::vector <std::unique_ptr<RangedAudioParameter>> m_paramVector;
	PresetHandler m_presets;
    float m_pluginScaleFactor = 1.0;    
#if WITH_MIDIKEYBOARD    
    MidiKeyboardState m_keyboardState;
    MidiModPitchBendState m_wheelState;
#endif
    jade::BypassDelay m_bypassDelay;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmplitudeHistogramAudioProcessor)
};
