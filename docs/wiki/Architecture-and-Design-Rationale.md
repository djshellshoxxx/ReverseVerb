# Architecture and Design Rationale

Sources: README ("Features"), `CHANGELOG.md`, `CMakeLists.txt`, the source tree. *(inferred)* marks conclusions from the layout.

- **Reverse-reverb as a render, then playback.** The swell is produced by a `RenderEngine` (`Source/dsp/`) with a reverb engine, then played through `Voices`. *(inferred from file names and the README's description of rendering and export.)*
- **Click-free parameter changes.** Parameter changes crossfade while a swell plays (README). *(from the feature list.)*
- **Golden render regression.** Output is compared against `Tests/data/golden.txt`, and regeneration is an explicit act (`RV_WRITE_GOLDEN=1`). This protects the DSP against unintended change.
- **Undo as a stack of states.** `UndoHistory` covers knobs, waveform edits, randomise and presets.
- **Drag-to-DAW and batch export share the render path.** `ExportMix.h` and `BatchExporter` reuse the engine (*inferred*).
- **PDC keeps the hit on the note.** The plugin reports latency so the dry hit lands on the MIDI note (README).
- **Proprietary going forward.** Earlier MIT-licensed versions remain under MIT (README "License").
