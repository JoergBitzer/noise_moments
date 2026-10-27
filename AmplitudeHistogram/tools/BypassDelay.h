/*
    BypassDelay.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: delays a buffer by a fixed number of samples, for processBlockBypassed().
    A plugin that reports a latency (setLatencySamples) must delay its bypassed signal by the same
    amount: the host compensates the latency also while the plugin is bypassed, otherwise the
    bypassed audio comes too early (and JUCE's default processBlockBypassed asserts).
    The synchronous block processing of the template delays by g_desired_blocksize_ms.
    Usage: prepare() in prepareToPlay (it allocates), process() in processBlockBypassed.
    License: MIT
*/
#pragma once
#include <algorithm>
#include <juce_audio_basics/juce_audio_basics.h>

namespace jade
{
class BypassDelay
{
public:
    void prepare(int numChannels, int delayInSamples)
    {
        m_delayInSamples = std::max(0, delayInSamples);
        m_memory.setSize(numChannels, std::max(1, m_delayInSamples));
        m_memory.clear();
        m_position = 0;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (m_delayInSamples == 0)
        {
            return;
        }
        const int numChannels = std::min(buffer.getNumChannels(), m_memory.getNumChannels());
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float delayed = m_memory.getSample(channel, m_position);
                m_memory.setSample(channel, m_position, buffer.getSample(channel, n));
                buffer.setSample(channel, n, delayed);
            }
            m_position = (m_position + 1) % m_delayInSamples;
        }
    }

private:
    juce::AudioBuffer<float> m_memory;
    int m_delayInSamples = 0;
    int m_position = 0;
};
} // namespace jade
