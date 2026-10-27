# CLAUDE.md -- instructions for Claude Code in noise_moments (two plugins made from the AdvancedAudioTemplate, AAT2)

## The project
Two JUCE audio plugins (VST3, AU on macOS, Standalone), each in its own folder, each a full AAT2 template copy:
- `MomentNoise/`: noise generator, skewness and excess kurtosis adjustable, pdf display (lower half).
- `AmplitudeHistogram/`: histogram of the input amplitudes, statistics, normal pdf and Johnson fit as overlay.
- `shared/`: code of both plugins -- `JohnsonDistribution.*` (design of the distribution, no JUCE),
  `DistributionDesigner.h` (background thread for the design), `PdfPlot.*` (the display); `shared/tests/`.
In each plugin folder (`<Plugin>` = MomentNoise or AmplitudeHistogram):
- `<Plugin>.h/.cpp`: the parameters (structs), the algorithm (`<Plugin>Audio`, direct processing:
  `g_desired_blocksize_ms` is 0, no latency) and the GUI (`<Plugin>GUI`). Most work happens here.
- `PluginProcessor.*`, `PluginEditor.*`: glue code (presets, state, window, preset bar); change rarely.
- `PluginSettings.h`: block size, GUI size, preset-bar and keyboard layout.
- `tools/`: template helpers -- `ParameterSpec.h`, `SwitchParameter.h`, `DayNightLookAndFeel.*`, `PresetHandler.*`,
  `BypassDelay.h`, `LogFrequencyRange.h`, `run_pluginval.*`, `with_virtual_display.sh`, `tester/`.
- `CMakeLists.txt`: version, plugin codes, switches, source list (incl. `../shared/*.cpp`).
- Docs: `README.md`, `HowToTestYourPlugin.md`, `docs/HowToDoc.md`, manual in `docs/manual/`.
Top level: `CMakeLists.txt` (JUCE + both plugins + `NOISE_MOMENTS_BUILD_TESTS`), `README.md`,
`release/ReadMeFirst.txt`, `.github/workflows/release.yml` (both plugins in one zip per system).

## Build
```console
git submodule update --init                      # JUCE (pinned submodule), once
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DAAT_BUILD_TESTER=ON -DNOISE_MOMENTS_BUILD_TESTS=ON
cmake --build build --target MomentNoise_VST3 MomentNoise_Tester AmplitudeHistogram_VST3 AmplitudeHistogram_Tester JohnsonDistributionTest
ctest --test-dir build
```
Artefacts: `build/<Plugin>/<Plugin>_artefacts/<Config>/VST3/<Plugin>.vst3`, Tester: `build/<Plugin>/<Plugin>_Tester`.
Develop with the Debug build (JUCE assertions are on); releases are Release builds made by CI.
For the review in a DAW, build Release as well (a Debug build is slow there and stops at assertions):
```console
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target MomentNoise_VST3 AmplitudeHistogram_VST3
```

## Workflow rules (from the author, follow them)
- Each new request gets a new git branch with a descriptive name (e.g. `feature/lowpass-filter`,
  `fix/preset-loading`). Commit all changes with clear, concise commit messages.
- Merge into main and push only when the user says so. Before merging, check the result
  (conflicts, conflict markers) before committing or continuing.
- Release tags (`vX.Y.Z`) only on the user's explicit request -- a tag triggers a public release.
- Versioning (`project(... VERSION X.Y.Z)` in the CMakeLists.txt of the changed plugin), once per logical change that gets
  committed: a new feature raises the second number and sets the third to 0 (1.0.3 -> 1.1.0); a fix or
  other change raises the third (1.1.0 -> 1.1.1). Documentation-only changes need no new version.
- Update the documentation (README, manual, controls list) in the same change as the code.
- Install for review, automatically: after a successful build and pluginval run, copy the
  **Release** build of the plugin into the user's plugin folder (replace the old copy) so the change
  can be checked in a DAW, and say so in the report. Linux: `~/.vst3/`; macOS:
  `~/Library/Audio/Plug-Ins/VST3/` and the AU to `~/Library/Audio/Plug-Ins/Components/`; Windows:
  `C:\Program Files\Common Files\VST3\` (needs admin rights; otherwise tell the user the path of
  the built plugin). Install the build that was tested (run pluginval on the Release build too).
- A change in `shared/` changes both plugins: raise both versions, test both. For a release, the
  top-level version and both plugin versions must be the same (the workflow checks it).
- Report results honestly: failed tests, skipped steps, things not tested (e.g. Windows/macOS).

## Testing without disturbing the author (the default)
The author works on the same machine. Everything that opens a window runs in a **virtual display**, never on the real desktop:
- pluginval: `<Plugin>/tools/run_pluginval.sh ...` -- on Linux it starts itself under `xvfb-run -a` if xvfb is installed
  (`AAT_REAL_DISPLAY=1` switches that off, only for a machine nobody works at);
- Tester snapshots, the Standalone and any other GUI program: `<Plugin>/tools/with_virtual_display.sh build/<Plugin>/<Plugin>_Tester snapshot gui.png ...`
  (it starts openbox inside the virtual display: JUCE's top-level windows end with an X error "BadAtom" on a bare Xvfb);
- never move the mouse, click, type into other windows or take screenshots of the desktop (xdotool, XTest, `import -window root`,
  computer-use, browser automation) without asking first and getting a yes for that specific action. Tests that need mouse or key
  events run inside the virtual display together with the program under test (start both under one `xvfb-run -a`), or hand the
  events to the window peer inside the Tester program (as the Tester's own options do);
- a real DAW can only be tried by the author: install the Release build (see above) and say what to try.
(On a desktop with mirrored monitors Debug GUI tests may assert in `juce_Displays.cpp`; in a virtual display they do not.)

## Before every commit
1. Build (Debug) without errors and warnings.
2. `<Plugin>/tools/run_pluginval.sh build/<Plugin>/<Plugin>_artefacts/Debug/VST3/<Plugin>.vst3` (both plugins)
   `ctest --test-dir build`
   -- all runs SUCCESS, zero JUCE assertions.
3. If the sound should not change (refactoring): render before and after with
   `<Plugin>_Tester render in.wav out.wav` and compare the files (identical = same sound).
4. After GUI changes (in a virtual display, `xvfb-run -a`): look at `<Plugin>_Tester snapshot gui.png` (both themes, see below).

## Conventions
- Readable code (code style of the author, binding): no one-liners, no magic numbers, no magic
  constants, no magic strings. Readability is more important than performance (but not too much).
- No ternary operators; no `if`/`else` inside expressions; no `goto`; no `using namespace` in headers.
- Constants with `constexpr`, not `#define`; no `#define` macros (inline functions or templates);
  JUCE's own macros are the exception.
- Headers include as little as possible: forward declare, include only what is used, no includes in
  headers just to pass them on.
- Member variables `m_...`, constants `kName`.
- Parameters: one struct per parameter in `<Plugin>.h` (ID, name, unit, range, default,
  `numDecimalPlaces`, `logFrequency`, `help`); create it with `jade::makeParameter(spec)`; show
  `jade::helpText(spec)` as tooltip. Don't build `AudioParameterFloat`s by hand.
- On/off parameters: not `juce::AudioParameterBool` (it keeps the raw value a host sends, so pluginval 2.x fails
  "Discrete parameter state restoration" at strictness 8 and above); use `jade::makeSwitchParameter(id, name, defaultIsOn)`
  from `tools/SwitchParameter.h` (a choice Off/On, snaps to a step; `ButtonAttachment` works unchanged).
- Controls: connect them with JUCE attachments (SliderAttachment etc.) or `juce::ParameterAttachment`;
  then automation, undo and the red "changed" Save button work (they use parameter gestures).
- Colours: take them from the LookAndFeel (`findColour`), so the day/night theme applies; fixed
  colours as 8-digit hex `juce::Colour(0xAARRGGBB)` (alpha first), not floats or `juce::Colours::` names.
- GUI text: only Latin-1 characters (no emoji, arrows, warning signs -- Windows fonts lack them); draw
  icons as `juce::Path` (example: `ThemeButton` in `tools/DayNightLookAndFeel.cpp`).
- Keep the algorithm free of GUI code (no `juce_gui_*` includes), so it can be tested headless.
- Factory presets: XML files embedded with `juce_add_binary_data` and `FACTORY_PRESETS`; add
  `presetversion="N"` to the root element and raise it when a preset changes (see README).
- The preset "Init" is written at every start with the default values of all parameters (`PresetHandler::writeInitPreset`);
  do not ship a factory preset called "Init".
- The plugin reports the latency of the synchronous blocks (`setLatencySamples` in `prepareToPlay`); `processBlockBypassed`
  delays an effect's input by the same amount (`tools/BypassDelay.h`). If the algorithm adds latency, add it to `m_Latency`.
- Per-user files (presets, `user.settings`) live in `PresetHandler::getUserPresetsFolder()`; nothing
  there may end in `.xml` except presets.

## Traps
- `PluginEditor.*` and `PluginProcessor.*` use Windows line endings (CRLF): keep them when editing.
- Never change anything in `JUCE/` (submodule).
- Tests must not touch the user's real presets and settings: on Linux/macOS run the Standalone, the
  Tester or pluginval with a temporary home folder (`HOME=$(mktemp -d) ...`; `run_pluginval.sh`
  does this itself). Day theme for a snapshot: write `user.settings` with
  `<VALUE name="dayTheme" val="1"/>` into the temporary preset folder.
- The workflow only builds on tags and "Run workflow" (main branch). To test CI changes on a branch,
  add a temporary `branches:` trigger and remove it again before merging.

## Releases (CI)
`.github/workflows/release.yml`: a tag `vX.Y.Z` that matches the version in CMakeLists.txt builds
Windows, macOS (Universal, VST3 + AU) and Linux (both plugins), tests with pluginval (and `auval` on macOS), and
creates the GitHub release with one zip per system (plugins, Standalone, `release/ReadMeFirst.txt`,
licenses, `<Plugin>/docs/manual/*.pdf`). Before a release: versions raised (top level and both plugins), manual updated
(`\pluginversion`, `controls.tex` via `<Plugin>_Tester --manual tex`), PDF rebuilt and committed.
