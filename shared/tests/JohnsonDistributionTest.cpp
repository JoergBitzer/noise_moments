/*
    JohnsonDistributionTest.cpp -- checks designJohnsonDistribution() on a grid of targets:
      1. the design reaches the target skewness and excess kurtosis (or reports "limited"),
      2. the pdf belongs to transform(): pdf(x(z)) * |dx/dz| = phi(z) (change of variables), and
         integrated numerically it has mean 0, variance 1 and the target moments. Only for smooth pdfs:
         SB close to the bound (delta < 0.5) has very narrow peaks, which the plain sum over x can not
         resolve, and x(z) gets so close to the bound that dx/dz can not be found numerically,
      3. noise made with transform() has these moments (sample estimate).
    Exit code 0 if all checks pass. No JUCE needed.
    License: MIT
*/
#include "../JohnsonDistribution.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>

namespace
{
constexpr double kMomentTolerance = 1e-3;     // design and pdf integral
constexpr double kPdfRange = 200.0;           // integrate the pdf over |x| < 200 sigma
constexpr int kPdfPoints = 400001;
constexpr int kNumSamples = 2000000;
constexpr double kConsistencyRangeZ = 4.0; // beyond, x is too close to a bound of SB for double precision
constexpr int kConsistencyPoints = 101;
constexpr double kDerivativeStep = 1e-5;
constexpr double kConsistencyTolerance = 1e-4; // relative
constexpr double kSampleSkewnessTolerance = 0.05; // sample estimates: rough check only
constexpr double kSampleSkewnessPerKurtosis = 0.02; // heavy tails: the estimate scatters more
constexpr double kSmoothDelta = 0.5;   // SB with delta above this: pdf smooth enough for the sum over x
constexpr double kSampleKurtosisRelTolerance = 0.15;

struct Estimate
{
    double integral = 0.0;
    double mean = 0.0;
    double variance = 0.0;
    double skewness = 0.0;
    double excess = 0.0;
};

Estimate momentsOfPdf(const jade::JohnsonDistribution& d)
{
    const double step = 2.0 * kPdfRange / (kPdfPoints - 1);
    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;
    double s3 = 0.0;
    double s4 = 0.0;
    for (int n = 0; n < kPdfPoints; ++n)
    {
        const double x = -kPdfRange + n * step;
        const double w = d.pdf(x) * step;
        s0 += w;
        s1 += w * x;
        s2 += w * x * x;
        s3 += w * x * x * x;
        s4 += w * x * x * x * x;
    }
    Estimate e;
    e.integral = s0;
    e.mean = s1;
    e.variance = s2;
    e.skewness = s3;
    e.excess = s4 - 3.0;
    return e;
}

// largest relative difference between pdf(x(z)) * |dx/dz| and the standard normal pdf
double pdfConsistencyError(const jade::JohnsonDistribution& d)
{
    const double pi = 3.14159265358979323846;
    double largest = 0.0;
    for (int n = 0; n < kConsistencyPoints; ++n)
    {
        const double z = -kConsistencyRangeZ + 2.0 * kConsistencyRangeZ * n / (kConsistencyPoints - 1);
        const double slope = (d.transform(z + kDerivativeStep) - d.transform(z - kDerivativeStep)) / (2.0 * kDerivativeStep);
        const double phi = std::exp(-0.5 * z * z) / std::sqrt(2.0 * pi);
        const double error = std::abs(d.pdf(d.transform(z)) * std::abs(slope) / phi - 1.0);
        largest = std::max(largest, error);
    }
    return largest;
}

Estimate momentsOfSamples(const jade::JohnsonDistribution& d)
{
    std::mt19937_64 generator(1234);
    std::normal_distribution<double> normal;
    double s1 = 0.0;
    double s2 = 0.0;
    double s3 = 0.0;
    double s4 = 0.0;
    for (int n = 0; n < kNumSamples; ++n)
    {
        const double x = d.transform(normal(generator));
        s1 += x;
        s2 += x * x;
        s3 += x * x * x;
        s4 += x * x * x * x;
    }
    Estimate e;
    e.integral = 1.0;
    e.mean = s1 / kNumSamples;
    e.variance = s2 / kNumSamples;
    e.skewness = s3 / kNumSamples;
    e.excess = s4 / kNumSamples - 3.0;
    return e;
}
} // namespace

int main()
{
    const double skewnessValues[] = { -2.0, -1.0, -0.3, 0.0, 0.01, 0.5, 1.0, 1.5, 2.0 };
    const double excessValues[] = { -1.8, -1.5, -1.2, -0.5, 0.0, 0.01, 0.5, 1.0, 3.0, 6.0, 10.0, 15.0 };
    int failures = 0;
    double longestMs = 0.0;
    for (double s : skewnessValues)
    {
        for (double k : excessValues)
        {
            const auto start = std::chrono::steady_clock::now();
            const jade::JohnsonDistribution d = jade::designJohnsonDistribution(s, k);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            longestMs = std::max(longestMs, ms);

            double wantedExcess = k;
            if (k < jade::smallestDesignableExcessKurtosis(s))
            {
                wantedExcess = jade::smallestDesignableExcessKurtosis(s);
            }
            bool ok = std::abs(d.skewness - s) < kMomentTolerance && std::abs(d.excessKurtosis - wantedExcess) < kMomentTolerance;
            const bool smooth = d.family != jade::JohnsonFamily::Bounded || d.delta > kSmoothDelta;
            const double consistency = pdfConsistencyError(d);
            const Estimate pdfMoments = momentsOfPdf(d);
            const bool pdfOk = consistency < kConsistencyTolerance && std::abs(pdfMoments.integral - 1.0) < kMomentTolerance && std::abs(pdfMoments.mean) < kMomentTolerance
                 && std::abs(pdfMoments.variance - 1.0) < kMomentTolerance
                 && std::abs(pdfMoments.skewness - s) < kMomentTolerance
                 && std::abs(pdfMoments.excess - wantedExcess) < 10.0 * kMomentTolerance;
            ok = ok && (pdfOk || !smooth);
            const Estimate sampleMoments = momentsOfSamples(d);
            ok = ok && std::abs(sampleMoments.skewness - s) < kSampleSkewnessTolerance + kSampleSkewnessPerKurtosis * std::abs(wantedExcess)
                 && std::abs(sampleMoments.excess - wantedExcess) < 0.1 + kSampleKurtosisRelTolerance * std::abs(wantedExcess);
            if (!ok)
            {
                ++failures;
            }
            std::printf("%s%s s=%5.2f k=%6.2f -> %-15s delta=%8.4f shape=%.6f  design s=%7.4f k=%7.4f%s | pdf err=%.1e int=%.5f "
                        "var=%.5f s=%7.4f k=%7.4f | samples s=%7.4f k=%7.4f | %6.1f ms\n",
                        ok ? "ok  " : "FAIL", smooth ? " " : "~", s, k, jade::familyName(d.family), d.delta, d.shape, d.skewness,
                        d.excessKurtosis, d.limited ? " (limited)" : "", consistency, pdfMoments.integral, pdfMoments.variance,
                        pdfMoments.skewness, pdfMoments.excess, sampleMoments.skewness, sampleMoments.excess, ms);
        }
    }
    std::printf("(~ = pdf not checked, see above)\n%d failures, longest design %.1f ms\n", failures, longestMs);
    return failures == 0 ? 0 : 1;
}
