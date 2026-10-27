# NoiseMoments: ReadMe

Two plugins:
- MomentNoise: noise generator with adjustable skewness and excess kurtosis (default: Gaussian
  noise at -24 dBFS RMS); the lower half of the window shows the theoretical pdf.
- AmplitudeHistogram: histogram of the input amplitudes, compared with the normal pdf and the
  pdf that fits the measured skewness and excess kurtosis.

## Check, if everything is OK
Since you read this file, you unzipped the downloaded file.

The directory should contain:
1. This ReadMeFirst.txt file (in MarkDown format, you can rename it in ReadMeFirst.md for a better view)
2. LICENSE and LICENSE-AGPL-3.0.txt, the licenses
3. the directories MomentNoise.vst3 and AmplitudeHistogram.vst3 (the VST3 plugins)
4. the Standalone applications (Windows: MomentNoise.exe, ...; macOS: MomentNoise.app, ...;
   Linux: MomentNoise, AmplitudeHistogram)
5. (macOS only) MomentNoise.component and AmplitudeHistogram.component (the AU plugins)
6. the manuals ManualMomentNoise.pdf and ManualAmplitudeHistogram.pdf

## Installation

### Windows
Copy the directories MomentNoise.vst3 and AmplitudeHistogram.vst3 to C:\Program Files\Common Files\VST3

### Mac VST3
Copy the directories MomentNoise.vst3 and AmplitudeHistogram.vst3 to
/Users/yourUSERNAME/Library/Audio/Plug-Ins/VST3

### Mac AU
Copy MomentNoise.component and AmplitudeHistogram.component to
/Users/yourUSERNAME/Library/Audio/Plug-Ins/Components

The macOS version is not signed with an Apple Developer ID yet. If macOS reports that the
plugin is damaged or cannot be opened, remove the quarantine flag in the Terminal, e.g.
xattr -cr ~/Library/Audio/Plug-Ins/VST3/MomentNoise.vst3

### Linux
Copy the directories MomentNoise.vst3 and AmplitudeHistogram.vst3 to
/home/yourUSERNAME/.vst3/

Done! Start your DAW and let it rescan the plugins.
The Standalone applications run without a DAW.

Warning: heavy-tailed noise (large excess kurtosis) has rare peaks far above its RMS level
(at the default level of -24 dBFS and the largest excess kurtosis, 100: up to about +15 dBFS).
Lower the level before you listen.

Have fun!

Joerg Bitzer


# Source code and license

## Download source code
You can download the source code at GitHub:

https://github.com/JoergBitzer/noise_moments

## License
- The source code is open source under the MIT License (see LICENSE),
  (c) Joerg Bitzer.
- The plugin binaries contain third-party code: JUCE (used under the AGPLv3), the VST3 SDK
  by Steinberg (MIT License) and, on macOS, the Audio Unit SDK by Apple (Apache License 2.0).
  Therefore the binaries as a whole are distributed under the GNU Affero General Public
  License v3 (see LICENSE-AGPL-3.0.txt). The complete source code is the repository above
  plus JUCE.

VST is a registered trademark of Steinberg Media Technologies GmbH.

The plugins come without any warranty (see the licenses).
