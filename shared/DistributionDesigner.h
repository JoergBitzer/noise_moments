/*
    DistributionDesigner.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-27
    Description: designs the Johnson distribution (JohnsonDistribution.h) for a target skewness and
    excess kurtosis in a background thread: a design takes up to about 80 ms, too long for the
    audio thread and too long for the message thread.
      - setTarget(): any thread, realtime safe (two atomics); the thread designs when the target changed
      - designNow(): designs the current target in the calling thread (prepareToPlay: the first block
        must already have the right distribution)
      - tryGetDesign(): audio thread, never blocks (returns false if the design is being published)
      - getDesign(): message thread (may wait for the short copy of the result)
    License: MIT
*/
#pragma once

#include <atomic>
#include <juce_core/juce_core.h>

#include "JohnsonDistribution.h"

namespace jade
{
class DistributionDesigner : private juce::Thread
{
public:
    DistributionDesigner()
        : juce::Thread("DistributionDesigner")
    {
        startThread();
    }

    ~DistributionDesigner() override
    {
        stopThread(kStopTimeoutMs);
    }

    void setTarget(float skewness, float excessKurtosis) noexcept
    {
        m_targetSkewness.store(skewness);
        m_targetExcessKurtosis.store(excessKurtosis);
    }

    void designNow()
    {
        designIfTargetChanged();
    }

    // version of the newest design: changes with every new design
    int getVersion() const noexcept
    {
        return m_version.load();
    }

    bool tryGetDesign(JohnsonDistribution& design, int& version) noexcept
    {
        const juce::SpinLock::ScopedTryLockType lock(m_publishLock);
        if (!lock.isLocked())
        {
            return false;
        }
        design = m_design;
        version = m_version.load();
        return true;
    }

    JohnsonDistribution getDesign()
    {
        const juce::SpinLock::ScopedLockType lock(m_publishLock);
        return m_design;
    }

    // the target the newest design was made for (the design may differ, see JohnsonDistribution::limited)
    float getDesignedSkewness()
    {
        const juce::SpinLock::ScopedLockType lock(m_publishLock);
        return m_designedSkewness;
    }

    float getDesignedExcessKurtosis()
    {
        const juce::SpinLock::ScopedLockType lock(m_publishLock);
        return m_designedExcessKurtosis;
    }

private:
    static constexpr int kStopTimeoutMs = 2000;
    static constexpr int kPollIntervalMs = 30;

    void run() override
    {
        while (!threadShouldExit())
        {
            designIfTargetChanged();
            wait(kPollIntervalMs);
        }
    }

    void designIfTargetChanged()
    {
        const juce::ScopedLock designLock(m_designLock); // designNow() and the thread
        const float skewness = m_targetSkewness.load();
        const float excessKurtosis = m_targetExcessKurtosis.load();
        if (m_hasDesign && juce::exactlyEqual(skewness, m_designedSkewness)
            && juce::exactlyEqual(excessKurtosis, m_designedExcessKurtosis))
        {
            return;
        }
        const JohnsonDistribution design = designJohnsonDistribution(skewness, excessKurtosis);
        jassert(design.isFinite());

        const juce::SpinLock::ScopedLockType publishLock(m_publishLock);
        m_design = design;
        m_designedSkewness = skewness;
        m_designedExcessKurtosis = excessKurtosis;
        m_hasDesign = true;
        m_version.store(m_version.load() + 1);
    }

    std::atomic<float> m_targetSkewness { 0.0f };
    std::atomic<float> m_targetExcessKurtosis { 0.0f };
    std::atomic<int> m_version { 0 };

    juce::CriticalSection m_designLock;
    juce::SpinLock m_publishLock;
    JohnsonDistribution m_design;          // guarded by m_publishLock
    float m_designedSkewness = 0.0f;       // guarded by m_publishLock (written also under m_designLock)
    float m_designedExcessKurtosis = 0.0f;
    bool m_hasDesign = false;
};
} // namespace jade
