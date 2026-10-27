/*
    JohnsonDistribution.cpp -- see JohnsonDistribution.h
    License: MIT
*/
#include "JohnsonDistribution.h"

#include <algorithm>
#include <cmath>

namespace jade
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

// distance to the bound skewness^2 - 2: there the distribution degenerates into two points
// (SB with delta -> 0). Closer than 0.5 the pdf has very narrow peaks, and the search gets slow
// and inaccurate; for skewness 0 the smallest excess is -1.5 (uniform: -1.2, sine: -1.5)
constexpr double kBoundMargin = 0.5;
// search range of delta (SU: small delta = very heavy tails; SB: small delta = bimodal)
constexpr double kMinDeltaUnbounded = 0.3;
constexpr double kMinDeltaBounded = 0.05;
constexpr double kMaxDelta = 60.0; // only for skewness 0: delta = 60 is practically normal
// a target this close to the lognormal line (or to 0/0) is taken as lognormal (or normal)
constexpr double kLognormalTolerance = 1e-4;
constexpr double kNormalTolerance = 1e-9;

// quadrature over z (trapezoidal rule, very accurate for the Gaussian weight)
constexpr double kBaseRangeZ = 8.5;    // |z| < 8.5 covers all but 1e-17 of the probability
constexpr double kTailRangePerDelta = 4.0; // the 4th moment of exp(z/delta) peaks at z = 4/delta
constexpr double kMaxRangeZ = 40.0;
constexpr double kMaxStepZ = 0.01;
constexpr double kStepsPerDelta = 8.0;  // SB with small delta: steep steps, at least 8 points per delta

// root search
constexpr int kMaxIterations = 100;
constexpr double kSkewnessTolerance = 1e-7;
constexpr double kKurtosisTolerance = 1e-6;
constexpr double kParameterTolerance = 1e-12;

struct Moments
{
    double mean = 0.0;
    double sigma = 1.0;
    double skewness = 0.0;
    double excessKurtosis = 0.0;
};

double standardNormalPdf(double z)
{
    return std::exp(-0.5 * z * z) / std::sqrt(2.0 * kPi);
}

// y(z) of the family, before offset and scale
double unitTransform(JohnsonFamily family, double delta, double shape, double z)
{
    const double v = z / delta;
    switch (family)
    {
    case JohnsonFamily::Normal:
        return z;
    case JohnsonFamily::Lognormal:
        return std::exp(v);
    case JohnsonFamily::Unbounded:
        return std::sinh(v) + shape * std::cosh(v);
    case JohnsonFamily::Bounded:
        return 1.0 / (std::exp(-v) + shape);
    }
    return z;
}

// density of y (before offset and scale)
double unitPdf(JohnsonFamily family, double delta, double shape, double y)
{
    switch (family)
    {
    case JohnsonFamily::Normal:
        return standardNormalPdf(y);
    case JohnsonFamily::Lognormal:
    {
        if (y <= 0.0)
        {
            return 0.0;
        }
        const double z = delta * std::log(y);
        return standardNormalPdf(z) * delta / y;
    }
    case JohnsonFamily::Unbounded:
    {
        // sinh(v) + t*cosh(v) = r*sinh(v + a) with r = sqrt(1 - t^2), a = atanh(t)
        const double r = std::sqrt(1.0 - shape * shape);
        const double a = std::atanh(shape);
        const double z = delta * (std::asinh(y / r) - a);
        return standardNormalPdf(z) * delta / std::sqrt(r * r + y * y);
    }
    case JohnsonFamily::Bounded:
    {
        // y = 1/(exp(-v) + c): 0 < y < 1/c
        if (y <= 0.0 || shape * y >= 1.0)
        {
            return 0.0;
        }
        const double z = -delta * std::log(1.0 / y - shape);
        return standardNormalPdf(z) * delta / (y * (1.0 - shape * y));
    }
    }
    return 0.0;
}

Moments computeMoments(JohnsonFamily family, double delta, double shape)
{
    const double rangeZ = std::min(kBaseRangeZ + kTailRangePerDelta / delta, kMaxRangeZ);
    const double wantedStep = std::min(kMaxStepZ, delta / kStepsPerDelta);
    const int numIntervals = static_cast<int>(std::ceil(2.0 * rangeZ / wantedStep));
    const double step = 2.0 * rangeZ / numIntervals;

    // one pass with raw moments around y(0) (close to the median, so the conversion to central
    // moments below does not lose precision)
    const double reference = unitTransform(family, delta, shape, 0.0);
    double sumWeights = 0.0;
    double sum1 = 0.0;
    double sum2 = 0.0;
    double sum3 = 0.0;
    double sum4 = 0.0;
    for (int n = 0; n <= numIntervals; ++n)
    {
        const double z = -rangeZ + n * step;
        double weight = standardNormalPdf(z);
        if (n == 0 || n == numIntervals)
        {
            weight *= 0.5;
        }
        const double d = unitTransform(family, delta, shape, z) - reference;
        const double d2 = d * d;
        sumWeights += weight;
        sum1 += weight * d;
        sum2 += weight * d2;
        sum3 += weight * d2 * d;
        sum4 += weight * d2 * d2;
    }
    const double m1 = sum1 / sumWeights;
    const double m2 = sum2 / sumWeights;
    const double m3 = sum3 / sumWeights;
    const double m4 = sum4 / sumWeights;

    const double variance = m2 - m1 * m1;
    const double mu3 = m3 - 3.0 * m1 * m2 + 2.0 * m1 * m1 * m1;
    const double mu4 = m4 - 4.0 * m1 * m3 + 6.0 * m1 * m1 * m2 - 3.0 * m1 * m1 * m1 * m1;

    Moments moments;
    moments.mean = reference + m1;
    moments.sigma = std::sqrt(variance);
    moments.skewness = mu3 / (variance * moments.sigma);
    moments.excessKurtosis = mu4 / (variance * variance) - 3.0;
    return moments;
}

// lognormal: omega = exp(1/delta^2)
double lognormalSkewness(double omega)
{
    return (omega + 2.0) * std::sqrt(omega - 1.0);
}

double lognormalExcessKurtosisOfOmega(double omega)
{
    return std::pow(omega, 4.0) + 2.0 * std::pow(omega, 3.0) + 3.0 * omega * omega - 6.0;
}

// solves (omega - 1)(omega + 2)^2 = skewness^2 (cubic, closed form)
double lognormalOmega(double skewness)
{
    const double s2 = skewness * skewness;
    const double root = std::sqrt(s2 + 0.25 * s2 * s2);
    return std::cbrt(1.0 + 0.5 * s2 + root) + std::cbrt(1.0 + 0.5 * s2 - root) - 1.0;
}

double deltaOfOmega(double omega)
{
    return 1.0 / std::sqrt(std::log(omega));
}

// Illinois method (regula falsi that does not get stuck): root of f in [a, b], f(a) and f(b)
// with different signs (given, so the end points need not be computed)
template <class Function>
double findRoot(Function f, double a, double b, double fa, double fb, double tolerance)
{
    int side = 0;
    double x = a;
    for (int iteration = 0; iteration < kMaxIterations; ++iteration)
    {
        x = (a * fb - b * fa) / (fb - fa);
        if (std::abs(b - a) < kParameterTolerance)
        {
            break;
        }
        const double fx = f(x);
        if (std::abs(fx) < tolerance)
        {
            break;
        }
        if ((fx > 0.0) == (fb > 0.0))
        {
            b = x;
            fb = fx;
            if (side == -1)
            {
                fa *= 0.5;
            }
            side = -1;
        }
        else
        {
            a = x;
            fa = fx;
            if (side == 1)
            {
                fb *= 0.5;
            }
            side = 1;
        }
    }
    return x;
}

// shape parameter of SU (t) or SB (c) from p in [0, 1]: p = 0 symmetric, p -> 1 lognormal
double shapeOfPosition(JohnsonFamily family, double p)
{
    if (family == JohnsonFamily::Bounded)
    {
        return 1.0 - p;
    }
    return p;
}

// for a given delta: the shape with the wanted skewness (>= 0); possible if the lognormal
// skewness of this delta is larger than the target
double shapeForSkewness(JohnsonFamily family, double delta, double skewness)
{
    if (skewness <= 0.0)
    {
        return shapeOfPosition(family, 0.0);
    }
    const double omega = std::exp(1.0 / (delta * delta));
    auto skewnessError = [family, delta, skewness](double p)
    {
        return computeMoments(family, delta, shapeOfPosition(family, p)).skewness - skewness;
    };
    const double errorAtSymmetric = -skewness;
    const double errorAtLognormal = lognormalSkewness(omega) - skewness;
    const double p = findRoot(skewnessError, 0.0, 1.0, errorAtSymmetric, errorAtLognormal, kSkewnessTolerance);
    return shapeOfPosition(family, p);
}

JohnsonDistribution standardized(JohnsonFamily family, double delta, double shape, bool mirrored)
{
    const Moments moments = computeMoments(family, delta, shape);
    JohnsonDistribution d;
    d.family = family;
    d.delta = delta;
    d.shape = shape;
    d.scale = 1.0 / moments.sigma;
    d.offset = -moments.mean / moments.sigma;
    d.skewness = moments.skewness;
    d.excessKurtosis = moments.excessKurtosis;
    if (mirrored)
    {
        d.scale = -d.scale;
        d.offset = -d.offset;
        d.skewness = -d.skewness;
    }
    return d;
}
} // namespace

double JohnsonDistribution::transform(double z) const
{
    return offset + scale * unitTransform(family, delta, shape, z);
}

double JohnsonDistribution::pdf(double x) const
{
    const double y = (x - offset) / scale;
    return unitPdf(family, delta, shape, y) / std::abs(scale);
}

double minimumExcessKurtosis(double skewness)
{
    return skewness * skewness - 2.0;
}

double smallestDesignableExcessKurtosis(double skewness)
{
    return minimumExcessKurtosis(skewness) + kBoundMargin;
}

double lognormalExcessKurtosis(double skewness)
{
    return lognormalExcessKurtosisOfOmega(lognormalOmega(skewness));
}

const char* familyName(JohnsonFamily family)
{
    switch (family)
    {
    case JohnsonFamily::Normal:
        return "SN (normal)";
    case JohnsonFamily::Lognormal:
        return "SL (lognormal)";
    case JohnsonFamily::Unbounded:
        return "SU (unbounded)";
    case JohnsonFamily::Bounded:
        return "SB (bounded)";
    }
    return "";
}

JohnsonDistribution designJohnsonDistribution(double skewness, double excessKurtosis)
{
    const bool mirrored = skewness < 0.0;
    const double s = std::abs(skewness);
    bool limited = false;
    double k = excessKurtosis;
    if (k < smallestDesignableExcessKurtosis(s))
    {
        k = smallestDesignableExcessKurtosis(s);
        limited = true;
    }

    if (s < kNormalTolerance && std::abs(k) < kNormalTolerance)
    {
        JohnsonDistribution normal;
        normal.limited = limited;
        return normal;
    }

    const double omega = lognormalOmega(s);
    const double kLognormal = lognormalExcessKurtosisOfOmega(omega);
    if (s > 0.0 && std::abs(k - kLognormal) < kLognormalTolerance)
    {
        JohnsonDistribution d = standardized(JohnsonFamily::Lognormal, deltaOfOmega(omega), 0.0, mirrored);
        d.limited = limited;
        return d;
    }

    JohnsonFamily family = JohnsonFamily::Bounded;
    double minDelta = kMinDeltaBounded;
    if (k > kLognormal)
    {
        family = JohnsonFamily::Unbounded;
        minDelta = kMinDeltaUnbounded;
    }

    // the largest delta: there the matching shape is the lognormal one (excess = kLognormal)
    double maxDelta = kMaxDelta;
    double errorAtMaxDelta = 0.0;
    if (s > 0.0)
    {
        maxDelta = deltaOfOmega(omega);
        errorAtMaxDelta = kLognormal - k;
    }
    else
    {
        errorAtMaxDelta = computeMoments(family, maxDelta, shapeOfPosition(family, 0.0)).excessKurtosis - k;
    }

    auto kurtosisError = [family, s, k](double delta)
    {
        const double shape = shapeForSkewness(family, delta, s);
        return computeMoments(family, delta, shape).excessKurtosis - k;
    };
    const double errorAtMinDelta = kurtosisError(minDelta);

    double delta = minDelta;
    if ((errorAtMinDelta > 0.0) == (errorAtMaxDelta > 0.0))
    {
        // the target lies beyond what this family reaches in the search range: take the edge
        limited = true;
        if (std::abs(errorAtMaxDelta) < std::abs(errorAtMinDelta))
        {
            delta = maxDelta;
            if (s > 0.0)
            {
                // the matching shape at this delta is the lognormal limit itself
                JohnsonDistribution d = standardized(JohnsonFamily::Lognormal, maxDelta, 0.0, mirrored);
                d.limited = limited;
                return d;
            }
        }
    }
    else
    {
        delta = findRoot(kurtosisError, minDelta, maxDelta, errorAtMinDelta, errorAtMaxDelta, kKurtosisTolerance);
    }
    const double shape = shapeForSkewness(family, delta, s);
    JohnsonDistribution d = standardized(family, delta, shape, mirrored);
    d.limited = limited;
    return d;
}
} // namespace jade
