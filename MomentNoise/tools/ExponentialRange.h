/*
    ExponentialRange.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-27
    Description: a NormalisableRange whose knob position u (0...1) grows exponentially:

        value = min + (max - min) * (exp(a*u) - 1) / (exp(a) - 1)

    with a chosen so that the middle of the knob (u = 0.5) gives centreValue. Unlike JUCE's skew
    (a power law, which crams the low end into a sliver), small values keep a usable share of the
    knob while the range still reaches far up. Example: -1.5 ... 100 with centre 3 puts 0 at 34 %,
    10 at 65 % and 25 at 78 % of the knob. centreValue must lie below the middle of the range.
    Values snap to numDecimalPlaces (also in the conversion from 0...1: the host's values are exact steps).

    Usage:
        jade::makeParameter(g_paramX, jade::makeExponentialRange(-1.5f, 100.0f, 3.0f, 2))

    The conversion functions clamp (see LogFrequencyRange.h): NormalisableRange asserts on any
    result outside [0, 1], even by a rounding error.
    License: MIT
*/
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace jade
{
inline juce::NormalisableRange<float> makeExponentialRange(float minValue, float maxValue, float centreValue,
                                                           int numDecimalPlaces)
{
    // u = 0.5 -> centre: (exp(a/2) - 1) / (exp(a) - 1) = 1 / (exp(a/2) + 1) = (centre - min) / (max - min)
    const double ratio = static_cast<double>(maxValue - minValue) / static_cast<double>(centreValue - minValue);
    jassert(ratio > 2.0); // the centre must lie below the middle of the range
    const double a = 2.0 * std::log(ratio - 1.0);
    const double expA = std::exp(a);
    const double step = std::pow(10.0, -numDecimalPlaces);

    return juce::NormalisableRange<float>(minValue, maxValue,
        [a, expA, step](float rangeStart, float rangeEnd, float normalised) // convertFrom0To1, snapped
        {
            // snapped here: juce::AudioParameterFloat::setValue does not snap, so a host's round trip
            // of the default 0 would give 1.2e-7 instead of 0
            const double u = juce::jlimit(0.0, 1.0, static_cast<double>(normalised));
            const double value = rangeStart + (rangeEnd - rangeStart) * (std::exp(a * u) - 1.0) / (expA - 1.0);
            const double snapped = std::round(value / step) * step;
            return juce::jlimit(rangeStart, rangeEnd, static_cast<float>(snapped));
        },
        [a, expA](float rangeStart, float rangeEnd, float value) // convertTo0To1
        {
            const double clamped = juce::jlimit(rangeStart, rangeEnd, value);
            const double relative = (clamped - rangeStart) / (rangeEnd - rangeStart);
            const double u = std::log(1.0 + relative * (expA - 1.0)) / a;
            return static_cast<float>(juce::jlimit(0.0, 1.0, u));
        },
        [step](float rangeStart, float rangeEnd, float value) // snapToLegalValue: clamp, then the step
        {
            const double snapped = std::round(juce::jlimit(rangeStart, rangeEnd, value) / step) * step;
            return juce::jlimit(rangeStart, rangeEnd, static_cast<float>(snapped));
        });
}
} // namespace jade
