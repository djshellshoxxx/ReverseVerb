# Installation troubleshooting

## Where the plugins go
`C:\Program Files\Common Files\VST3\ReverseVerb.vst3` and `...\ReverseVerb FX.vst3` (each is a folder). macOS: `~/Library/Audio/Plug-Ins/VST3` and `.../Components` for AU.

## FL Studio doesn't see the plugin
1. Options > Manage plugins > tick "VST3" and make sure `C:\Program Files\Common Files\VST3` is in the search paths (it is by default).
2. Click "Find plugins" (or "Find more plugins"). Not "Verify plugins" alone.
3. Look under **Installed > Generators** for ReverseVerb and **Installed > Effects** for ReverseVerb FX. Tick the star to make them favourites so they show in the Add menu.
4. Still missing: confirm the folder actually contains `Contents\x86_64-win\ReverseVerb.vst3`. If it only contains a stray nested `ReverseVerb.vst3` folder, delete the whole thing and rerun `build.ps1`.

## "Plugin failed to load" / scan crash
- You built a Debug or 32-bit build. Rebuild with `build.ps1` (Release, x64). FL 64-bit needs 64-bit plugins.
- The Visual C++ runtime is missing on a different machine: install "Microsoft Visual C++ Redistributable 2015-2022 x64".
- Antivirus quarantined the .vst3 during the build. Add an exclusion for the VST3 folder and rebuild.

## No sound (instrument)
- Nothing loaded: the waveform says so. LOAD, drop a file, or GENERATE.
- Notes are in the piano roll but nothing plays: the channel is muted, or the mixer track is routed nowhere.
- With "Hit on note (PDC)" on, FL delays other tracks to line things up; if you hear the swell late, turn PDC off and place the notes earlier, or check Options > Audio > "Plugin delay compensation".

## FX: swells don't line up / arrive late
- Plugin delay compensation must be on in FL (Mixer > track options and Options > Audio). The FX reports its latency; the DAW must honour it.
- Latency changes when you change LENGTH, DELAY, HIT LENGTH, PITCH, STRETCH or SYNC. Tweak those while stopped; FL re-syncs on the next play.
- Status shows "missed": swell longer than budget. Lower LENGTH or raise HOLD.

## FX: no triggers
- Input too quiet for THRESHOLD: drag the dashed line in the scope down.
- SENSITIVITY too high for busy material: lower it.
- TRIGGER is set to MIDI but no notes reach the plugin.
- After a trigger the detector re-arms only once the level drops (hysteresis). Sustained material won't retrigger; that's intended.

## Presets / exports
- Presets live in `Documents\ReverseVerb\...`. If SAVE does nothing, that folder isn't writable (OneDrive-redirected Documents sometimes need a moment; retry).
- EXPORT writes a temp file and renames it; if the export fails your old file is untouched. Check the destination isn't open in another program.
- DRAG TO DAW writes to `%TEMP%\ReverseVerb\`. Files older than an hour are cleaned up automatically.

## Uninstall
Delete the two `.vst3` folders and `Documents\ReverseVerb`. Rescan in FL.
