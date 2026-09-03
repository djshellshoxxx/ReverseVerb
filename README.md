# ReverseVerb

Reverse-reverb swell generator for snares, hats, claps (or any one-shot). Built for DnB / dubstep / breaks. VST3 + AU + Standalone, made with JUCE.

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
- Host-aware knob menus: right-click a parameter for DAW automation commands where supported, plus Reset to default
- Drag-to-DAW, WAV export, random reverb, built-in help (`?`)

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

## FL Studio
Options > Manage plugins > Find plugins. Add ReverseVerb to the Channel Rack as an instrument. Notes in the piano roll trigger it.

## License
MIT
