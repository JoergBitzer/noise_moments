# noise_moments

Two audio plugins (VST3, AU on macOS, Standalone) about the amplitude distribution of signals,
made from the [AdvancedAudioTemplate](https://github.com/JoergBitzer/AdvancedAudioTemplate), branch AAT2:

| Plugin | What it does |
|---|---|
| [MomentNoise](MomentNoise/README.md) | Noise generator: skewness and excess kurtosis of the noise are adjustable, default Gaussian noise at -24 dBFS RMS. The lower half of the window shows the theoretical pdf. |
| [AmplitudeHistogram](AmplitudeHistogram/README.md) | Histogram of the input amplitudes (an estimate of the pdf) with mean, RMS, skewness and excess kurtosis, compared with the normal pdf and the pdf that fits the measured moments. |

Both display the density over the standardized amplitude (x - mean) / sigma from -6 to 6 sigma, so the
histogram of MomentNoise's output can be compared directly with MomentNoise's theoretical pdf.

## How the noise is made: the Johnson system
A standard normal variable z is transformed, x = xi + lambda * y(z), with one of four families
(N. L. Johnson, 1949): SN (normal), SL (lognormal, y = exp(z/delta)), SU (unbounded,
y = sinh(z/delta) + t cosh(z/delta), heavy tails) and SB (bounded, y = 1/(exp(-z/delta) + c), light
tails). Together they cover every possible pair of skewness s and excess kurtosis k (k > s^2 - 2), the
pdf has a closed form, and noise is made by transforming Gaussian noise. The parameters are found
numerically (moments by quadrature, two nested root searches) in a background thread
(`shared/JohnsonDistribution.*`, `shared/DistributionDesigner.h`). MomentNoise keeps a distance of 0.5
to the bound (k >= s^2 - 1.5): closer, the pdf degenerates into two narrow peaks.

## Layout
```
CMakeLists.txt          builds both plugins (JUCE from the submodule, or from a parent project)
JUCE/                   git submodule, JUCE 9.0.3
MomentNoise/            plugin 1 (a full AAT2 template copy: tools/, docs/manual/, ...)
AmplitudeHistogram/     plugin 2
shared/                 Johnson distribution, designer thread, pdf plot (used by both)
shared/tests/           test of the distribution design (no JUCE)
release/ReadMeFirst.txt goes into the release zips
.github/workflows/      release build for Windows, macOS, Linux (one zip per system, both plugins)
```

## Build
```console
git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DAAT_BUILD_TESTER=ON -DNOISE_MOMENTS_BUILD_TESTS=ON
cmake --build build --target MomentNoise_VST3 AmplitudeHistogram_VST3 MomentNoise_Tester AmplitudeHistogram_Tester JohnsonDistributionTest
ctest --test-dir build                    # distribution design test
MomentNoise/tools/run_pluginval.sh build/MomentNoise/MomentNoise_artefacts/Debug/VST3/MomentNoise.vst3
```
Testing (pluginval, Tester snapshots and renders): see `MomentNoise/HowToTestYourPlugin.md`.

## Release
A tag `vX.Y.Z` builds and releases both plugins; it must match the version in `CMakeLists.txt` and in
both plugin folders (`MomentNoise/CMakeLists.txt`, `AmplitudeHistogram/CMakeLists.txt`).

## License
Source code: MIT (see LICENSE). The binaries contain JUCE (AGPLv3), so they are distributed under the
AGPLv3 (see LICENSE-AGPL-3.0.txt).
