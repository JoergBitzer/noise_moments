/*
    JohnsonDistribution.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-27
    Description: a distribution with a given skewness and excess kurtosis (mean 0, variance 1),
    taken from the Johnson system (N. L. Johnson, 1949). A standard normal variable z is transformed:

        x = offset + scale * y(z)

    with one of four families (the names of Johnson):
      SN  normal       y = z                                  skewness 0, excess 0
      SL  lognormal    y = exp(z/delta)                       on the "lognormal line"
      SU  unbounded    y = sinh(z/delta) + t*cosh(z/delta)    above the lognormal line (heavy tails)
      SB  bounded      y = 1/(exp(-z/delta) + c)              below the lognormal line (light tails)

    (t = tanh(gamma/delta) and c = exp(-gamma/delta) are Johnson's gamma in a form without overflow.)
    Together they cover every possible pair of skewness s and excess kurtosis k (k > s^2 - 2), so the
    two can be chosen independently. The pdf has a closed form, and noise is made by transforming
    Gaussian noise (no table, no rejection).

    designJohnsonDistribution() finds delta and t (or c) numerically (moments by quadrature over z,
    two nested root searches). It takes some ten milliseconds: call it outside the audio thread
    (see DistributionDesigner.h). No JUCE in here, so it can be tested on its own.
    License: MIT
*/
#pragma once

namespace jade
{
enum class JohnsonFamily
{
    Normal,    // SN
    Lognormal, // SL
    Unbounded, // SU
    Bounded    // SB
};

struct JohnsonDistribution
{
    JohnsonFamily family = JohnsonFamily::Normal;
    double delta = 1.0;  // tail shape: large delta = close to normal
    double shape = 0.0;  // SU: t in [0, 1), SB: c in (0, 1], otherwise unused
    double offset = 0.0; // x = offset + scale * y(z): mean 0, variance 1
    double scale = 1.0;  // negative for a negative skewness (mirrored)

    double skewness = 0.0;       // what the design reached (the target, or the next possible value)
    double excessKurtosis = 0.0;
    bool limited = false;        // true: the target excess kurtosis was not possible, it was moved

    // standard normal value z -> value x of this distribution (mean 0, variance 1)
    double transform(double z) const;
    // probability density at x
    double pdf(double x) const;
};

// the theoretical bound: every distribution has an excess kurtosis > skewness^2 - 2
double minimumExcessKurtosis(double skewness);
// the smallest excess kurtosis the design allows (a little above the bound, see the .cpp)
double smallestDesignableExcessKurtosis(double skewness);
// excess kurtosis of the lognormal distribution with this skewness (border between SU and SB)
double lognormalExcessKurtosis(double skewness);
// short name of the family, e.g. "SU (unbounded)"
const char* familyName(JohnsonFamily family);

// the distribution with this skewness and excess kurtosis (mean 0, variance 1)
JohnsonDistribution designJohnsonDistribution(double skewness, double excessKurtosis);
} // namespace jade
