# ReverseVerb

Reverse-reverb swell generator for snares, hats, claps (or any one-shot). Built for DnB / dubstep / breaks. VST3 + CLAP + Standalone (AU on macOS), made with JUCE.

Load a hit, dial in the reverb, and you instantly get a reversed-reverb swell that rises into the hit. Browse a whole folder of samples with `<` `>`, drag the result straight into your DAW, or export a WAV.

## Features
- Custom reverb: size, decay, damp, diffusion, early reflections, stereo separation, width, pre-hit delay
- Swell shaping: length, shape curve, color (low pass), bass cut (high pass)
- Waveform colour reacts to the Color and Bass Cut knobs; the SPACE panel shows a wireframe that reflects diffusion / size / decay
- Click the waveform to play; drag to trim start / end (cut the hit off for a pure swell)
- Volume envelope with tension, drawn over the waveform (FL automation style)
- Pitch sweep with 1 / 2 / 4 octave range and tension curve
- Sync total length to host BPM: 1 / 2 / 4 / 8 beats or 1 / 2 / 4 bars, with beat lines on the waveform
- Hit on note (PDC) so the dry hit lands exactly on the MIDI note
- Live readout of time, pitch and volume during playback
- Drag-to-DAW, random reverb, built-in help (`?`)
- Presets: factory + user presets, save/overwrite, dirty marker, A/B compare
- Keytrack: play the swell chromatically (root note, with PDC-aware hit alignment)
- Output gain, safety soft limiter and a peak meter with clip light
- Undo / redo (Ctrl/Cmd+Z, Ctrl/Cmd+Shift+Z) for knobs, waveform edits, randomize and presets
- Batch export: render a whole folder of samples with the current settings (16/24/32-bit, sample rate, normalize, name pattern)
- Resizable window (75-150%), size remembered per project
- Click-free: parameter changes crossfade while a swell is playing; deterministic offline bounces

## Build (Windows)
1. Clone or download, keep the path short (e.g. `C:\ReverseVerb`)
2. Right-click `build.ps1` > Run with PowerShell (as Administrator). First run installs the compiler and downloads JUCE.
3. Plugin lands in `C:\Program Files\Common Files\VST3\ReverseVerb.vst3`

If PowerShell blocks scripts: `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned` once in an admin PowerShell.

## Build (macOS / Linux)
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Tests
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ReverseVerbTests
build/ReverseVerbTests_artefacts/Release/ReverseVerbTests          # golden render, fuzz, voices, presets, undo, batch
RV_FUZZ_N=600 ...                                                  # longer parameter fuzz
RV_UI_TESTS=1 RV_SNAPSHOT_DIR=/tmp/shots xvfb-run -a ...           # editor render tests (needs a display)
```
CI runs the tests on Windows and Linux, plus an AddressSanitizer/UBSan run and pluginval. The golden baseline is in `Tests/data/golden.txt` (regenerate deliberately with `RV_WRITE_GOLDEN=1`). Feature specs and the roadmap are in `docs/specs/`.

## FL Studio
Options > Manage plugins > Find plugins. Add ReverseVerb to the Channel Rack as an instrument. Notes in the piano roll trigger it.

## License

Current and future development is proprietary. See `LICENSE`.

Earlier versions and commits that were published under the MIT License remain available under the MIT terms that accompanied those versions.

## Required shared plug-in standard

This project follows the [Circuit Drift Labs Shared Audio Plugin Standard](docs/standards/CDL_PLUGIN_BASELINE.md). It is required for the plug-in target; standalone-only requirements apply only when a standalone target is included. The product-specific specification supplements the shared standard and records the applicable profiles, compliance status, and any exceptions.
