# ReverseVerb 2.0

Tempo-shaped reverb rises and falls for snares, hats, claps, and other one-shots. ReverseVerb builds VST3 and Standalone formats on Windows/Linux, plus VST3, AU, and Standalone on macOS with JUCE 8.0.4.

Load a hit, choose **Rise** for reversed reverb before the transient or **Fall** for a forward decay after it, then trigger the result from MIDI. Browse a sample folder with `<` `>`, drag the processed result into your DAW, or export a WAV.

## Version 2 features

- Rise/Fall direction engine with direction-aware Delay and waveform placement
- Rise-mode plugin delay compensation (PDC) so the dry hit can land on the MIDI note; Fall reports zero lookahead
- Fine sync divisions from 1/64 triplet through sixteen bars, including straight, triplet, and dotted values
- A free (unsynced) Length knob that stretches the tail up to 64 seconds
- Time-signature-aware bars and waveform grids using the host BPM and meter
- Built-in sample generator: synthesize a snare, hi-hat, or clap one-shot to feed straight into the Rise/Fall engine
- 16/32-step tempo-synced gator with variable step levels
- Seven gate shapes: Square, Smooth, Ramp Up, Ramp Down, Triangle, Sine, and Curved
- Gate Depth, Smooth, Swing, Phase, Note/Host retrigger, and Swell/Hit/Both targeting
- Click/drag pattern painting and Shift-drag line drawing
- Clear, Fill, Invert, Randomize, rotate left/right, validated Copy/Paste, and Undo/Redo
- Automatable gator controls with host context menus where the DAW exposes them
- One shared gate/mix path for live playback and WAV export
- Version 1 sync-state migration and stable legacy parameter IDs

## Core features

- Custom reverb: size, decay, damp, diffusion, early reflections, stereo separation, width, pre-hit delay
- Swell shaping: length, shape curve, color (low pass), bass cut (high pass)
- Waveform colour reacts to the Color and Bass Cut knobs; the SPACE panel shows a wireframe that reflects diffusion / size / decay
- Click the waveform to play; drag to trim start / end (cut the hit off for a pure swell)
- Volume envelope with tension, drawn over the waveform (FL automation style)
- Pitch sweep with 1 / 2 / 4 octave range and tension curve
- Sync total length to the host BPM and time signature, with musical grid lines on the waveform
- Hit on note (PDC) so the dry hit lands exactly on the MIDI note
- Live readout of time, pitch and volume during playback
- Host-aware controls: right-click any knob, toggle, dropdown, waveform trim handle, or the pitch tension box for DAW automation commands where supported, plus Reset to default
- Drag-to-DAW, WAV export, random reverb, built-in help (`?`)

## Reproducible developer build

Requirements: CMake 3.22+, a C++17 compiler, and the normal JUCE platform development packages. The bootstrap scripts create a local Python environment with pinned CMake/Ninja versions when those tools are unavailable.

Linux/macOS:

```sh
./scripts/bootstrap-dev.sh debug
cmake --build --preset build-debug --target ReverseVerbTests ReverseVerb_VST3 ReverseVerb_Standalone
ctest --preset test-debug
```

Windows PowerShell:

```powershell
.\scripts\bootstrap-dev.ps1 windows
cmake --build --preset build-windows-debug --target ReverseVerbTests ReverseVerb_VST3 ReverseVerb_Standalone
ctest --preset test-windows-debug
```

Use `release`, `build-release`, and `test-release` for release builds on Linux/macOS. Use `build-windows-release` and `test-windows-release` on Windows. macOS also provides the `ReverseVerb_AU` target.

To reuse an existing JUCE 8.0.4 checkout instead of downloading it:

```sh
cmake --preset release -DRV_JUCE_SOURCE_DIR=/absolute/path/to/JUCE
```

## Automated verification

Every v2 branch and pull request builds Debug and Release on Windows, macOS, and Linux, runs the JUCE/CTest regression suite, verifies pinned pluginval 1.0.4 archives by SHA-256, and validates VST3 (plus AU on macOS) at strictness level 5. Release plugins and validator logs are uploaded as workflow artifacts.

If PowerShell blocks scripts: `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned` once in an admin PowerShell.

## FL Studio

Open **Options > Manage plugins > Find plugins**, then add ReverseVerb to the Channel Rack as an instrument. Piano-roll notes trigger it and velocity controls voice level.

Right-click menus are provided through the standard host parameter-context API. FL Studio may add commands such as **Create automation clip** when it exposes them for third-party VST3 parameters. FL-specific stock-plugin-only commands are not emulated by ReverseVerb.

See [`docs/v2-fl-studio-test-checklist.md`](docs/v2-fl-studio-test-checklist.md) for the manual host acceptance pass.

## License
MIT
