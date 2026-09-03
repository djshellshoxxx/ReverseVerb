# ReverseVerb

Two plugins: **ReverseVerb** (instrument: load a hit, get a swell) and **ReverseVerb FX** (effect: drop it on a drum track and every hit gets a swell in real time via PDC).

Reverse-reverb swell generator for snares, hats, claps (or any one-shot). Built for DnB / dubstep / breaks. VST3 + AU + Standalone, made with JUCE.

Load a hit, dial in the reverb, and you instantly get a reversed-reverb swell that rises into the hit. Browse a whole folder of samples with `<` `>`, drag the result straight into your DAW, or export a WAV.

## v1.2
Modes (reverse / forward / dry), generated hits, stretch, echo/chorus FX, pan envelope, gate with hard/smooth/LFO shapes and shuffle, sync to 32 bars, bass/treble, normalize, swap L/R, invert phase, knob right-click menus, presets, icons.

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
- Drag-to-DAW, WAV export, random reverb, built-in help (`?`)

## ReverseVerb FX
Transient-triggered: threshold / sensitivity / hold / hit length / gate, MIDI or audio triggering, freeze, listen, follow level, boost, max overlapping swells, trigger scope, latency readout. Reports latency = swell length so the DAW's PDC lines everything up (not for live input).

## Presets
Factory presets built in; your own are saved in `Documents\ReverseVerb\Instrument` and `Documents\ReverseVerb\FX`.

## Docs
- `docs/HELP.md` user guide (also the `?` button in the plugins)
- `docs/INSTALL_TROUBLESHOOTING.md`
- `docs/BUILDING.md` compiling from source

## Build (Windows)
1. Clone or download, keep the path short (e.g. `C:\ReverseVerb`)
2. Right-click `build.ps1` > Run with PowerShell (as Administrator). First run installs the compiler and downloads JUCE.
3. Both plugins land in `C:\Program Files\Common Files\VST3\`

If PowerShell blocks scripts: `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned` once in an admin PowerShell.

## Build (macOS / Linux)
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## FL Studio
Options > Manage plugins > Find plugins. Add ReverseVerb to the Channel Rack as an instrument (notes trigger it). Add ReverseVerb FX to a mixer insert on a drum track.

## License
MIT
