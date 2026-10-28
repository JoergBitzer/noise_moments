# noise_moments: two plugins made from the AdvancedAudioTemplate (AAT2)
#  - MomentNoise:        noise generator with adjustable skewness and excess kurtosis
#  - AmplitudeHistogram: histogram of the input amplitudes (estimate of the pdf)
#
# Standalone build of this repository (also used by the GitHub release workflow):
#
#   git clone --recursive <this repository> noise_moments
#   cmake -S noise_moments -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release --target MomentNoise_VST3 AmplitudeHistogram_VST3
#
# JUCE comes as a git submodule (JUCE/, pinned to a JUCE release). Inside AudioDev, the two plugin
# folders can also be added directly with add_subdirectory (then AudioDev's JUCE is used).

cmake_minimum_required(VERSION 3.22)

project(NoiseMoments VERSION 0.2.1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # needed on Linux for binary data (e.g. factory presets)

if(NOT TARGET juce::juce_core) # JUCE not added by a parent project: use the submodule
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        message(FATAL_ERROR "JUCE not found: run 'git submodule update --init' first")
    endif()
    add_subdirectory(JUCE)
endif()

add_subdirectory(MomentNoise)
add_subdirectory(AmplitudeHistogram)

# test of the distribution design (no JUCE): cmake ... -DNOISE_MOMENTS_BUILD_TESTS=ON, then ctest
option(NOISE_MOMENTS_BUILD_TESTS "Build the test of the Johnson distribution design" OFF)
if(NOISE_MOMENTS_BUILD_TESTS)
    enable_testing()
    add_executable(JohnsonDistributionTest shared/tests/JohnsonDistributionTest.cpp shared/JohnsonDistribution.cpp)
    add_test(NAME JohnsonDistributionTest COMMAND JohnsonDistributionTest)
endif()
