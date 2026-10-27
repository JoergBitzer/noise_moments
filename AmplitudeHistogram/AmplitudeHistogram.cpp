#include <math.h>
#include "AmplitudeHistogram.h"

#include "PluginProcessor.h"

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
    // here your code

}

int AmplitudeHistogramAudio::processSynchronBlock(juce::AudioBuffer<float> & buffer, juce::MidiBuffer &midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(buffer, midiMessages, NrOfBlocksSinceLastProcessBlock);
    return 0;
}

void AmplitudeHistogramAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>> &paramVector)
{
    // this is just a placeholder (necessary for compiling/testing the template):
    // one line per parameter, made from its definition in AmplitudeHistogram.h (see tools/ParameterSpec.h)
    paramVector.push_back(jade::makeParameter(g_paramExample));

}

void AmplitudeHistogramAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState> &vts)
{
    juce::ignoreUnused(vts);
}


AmplitudeHistogramGUI::AmplitudeHistogramGUI(AmplitudeHistogramAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
:m_processor(p) ,m_apvts(apvts)
{
    // A control for a parameter, e.g. a knob (members: juce::Slider m_exampleSlider; and
    // std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_exampleAttachment;):
    //   m_exampleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
    //       m_apvts, g_paramExample.ID, m_exampleSlider);  // range, value and text come from the parameter
    //   m_exampleSlider.setTooltip(jade::helpText(g_paramExample)); // shown when the mouse rests on it
    //   addAndMakeVisible(m_exampleSlider);
}

void AmplitudeHistogramGUI::paint(juce::Graphics &g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).brighter(0.3f));

    // Take colours from the LookAndFeel (findColour), not fixed ones like juce::Colour(0xffffffff):
    // then your GUI follows the day/night theme (WITH_DAYNIGHT) and any other LookAndFeel.
    g.setColour (getLookAndFeel().findColour (juce::Label::textColourId));
    g.setFont (12.0f);
    
    juce::String text2display = "AmplitudeHistogram V " + juce::String(PLUGIN_VERSION_MAJOR) + "." + juce::String(PLUGIN_VERSION_MINOR) + "." + juce::String(PLUGIN_VERSION_PATCH);
    g.drawFittedText (text2display, getLocalBounds(), juce::Justification::bottomLeft, 1);

}

void AmplitudeHistogramGUI::resized()
{
	auto r = getLocalBounds();
    
    // if you have to place several components, use scaleFactor
    //int width = r.getWidth();
	//float scaleFactor = float(width)/g_minGuiSize_x;

    // use the given canvas in r
    juce::ignoreUnused(r);
}
