# ReverseVerb — Feature Specs and Implementation Plan

Status: proposal v1 (written against `master` @ c98cbac). Audience: developers implementing the roadmap.
Everything below is grounded in the current code (`Source/PluginProcessor.*`, `Source/PluginEditor.*`, `CMakeLists.txt`).
Items marked **[SPIKE]** rest on an assumption about a library behaviour that was not verified; do the spike first.

---------------------------------------------------------------------------------------------------

## 0. Read this first: how to avoid bugs and merge conflicts

### 0.1 Current architecture (facts the specs depend on)
- `ReverseVerbProcessor` (JUCE `AudioProcessor`, instrument, stereo/mono out, MIDI in).
- Parameters live in one `AudioProcessorValueTreeState` (`apvts`), created in `createLayout()`. All IDs are in `namespace IDs` (`PluginProcessor.h`).
- Sound = one pre-rendered buffer (`RenderedSample`): reverb → reverse → filters → shape → swell+gap+hit → trim → pitch → volume envelope. Rendered on a **background thread** (`RenderThread`), stage 1-7 cached in `RenderCache` keyed by 17 doubles. Result swapped under `renderLock` (SpinLock); old buffers parked in `retired[2]` so the audio thread never frees them.
- Playback (`processBlock` → `renderRange`) mixes up to 8 `Voice`s reading the buffer; `hitIndex` splits swell (× Swell gain) from hit (× Hit gain).
- State: `getStateInformation` = apvts state + property `file`.
- GUI: single `ReverseVerbEditor`, fixed 1060×720, layout hard-coded in `resized()`. Colours are constants in `namespace RVColours`.

### 0.2 Threading rules (enforce in review)
| Thread | Allowed | Forbidden |
|---|---|---|
| Audio (`processBlock`) | atomics, `SpinLock` **try**-lock, reading a `shared_ptr` copy, fixed arrays | allocation, `new`/`std::vector` growth, blocking locks, file I/O, freeing a `RenderedSample` |
| Render thread | allocation, heavy DSP, `renderMutex`, `sourceLock` | touching GUI, calling `setLatencySamples` directly (use `pendingLatency`) |
| Message thread | GUI, file dialogs, preset I/O, `setLatencySamples` | long DSP (>5 ms), holding `renderMutex` |

### 0.3 Compatibility rules (breaking these corrupts users' projects)
1. **Never change** `PLUGIN_CODE Rvrb`, `PLUGIN_MANUFACTURER_CODE Shdv`, existing parameter IDs, their ranges, or choice-list **order**. Append only.
2. New parameters use `ParameterID { id, 2 }` (version hint 2) and **defaults that reproduce current behaviour**, so old projects sound identical.
3. Add a `stateVersion` int property (starts at 2 with the first feature that ships) to saved state; `setStateInformation` must tolerate missing properties and unknown ones.
4. Reserved new IDs (so parallel branches never collide): `keytrack`, `rootNote`, `outGain`, `limiter`, `algo`, `humanize`, `velSens`, `velBalance`, `sweepAmt`, `swellWidth`, `drive`, `reverbSource`. Reserved state properties: `stateVersion`, `uiW`, `uiH`, `presetName`, `irFile`, `locks`.

### 0.4 Merge-conflict strategy (the most important section for speed)
`PluginProcessor.cpp` (~600 lines) and `PluginEditor.cpp` (~950 lines) are conflict hotspots: every feature edits `createLayout()`, `render()`, `processBlock()` and `resized()`. **Do PR-0a first, alone, and freeze feature branches until it merges.**

**PR-0a — pure refactor, zero behaviour change** (target ≤ 1 day, review with a golden-render diff):
- `Source/dsp/ReverbEngine.h/.cpp` — move `Comb`, `Allpass`, `ReverbEngine` out of the anonymous namespace.
- `Source/dsp/RenderEngine.h/.cpp` — extract `render()` into a pure function:
  `RenderedSample renderSample(const RenderSettings&, const SourceAudio&, RenderCache*)` where `RenderSettings` is a plain struct snapshot of every parameter + `sampleRate` + `bpm`. **No access to `apvts`, no globals.** The processor builds the struct from `param(...)`. This is required by Batch Export and by tests.
- `Source/Voices.h/.cpp` — `Voice` + mixing loop (`renderRange`).
- `Source/Params.h/.cpp` — `IDs` and `createLayout()`; split into `addReverbParams(p)`, `addSwellParams(p)` … so each feature appends a function call on its own line region.
- `Source/ui/` — one header/cpp per component (`WaveformDisplay`, `DiffusionShape`, `TensionBox`, `DragOutPad`, `HelpOverlay`, `RVLookAndFeel`, `Theme`).
- `CMakeLists.txt`: list sources one per line, alphabetical (merge-friendly).
- Verify: golden test (PR-0b) shows bit-identical output before/after.

**PR-0b — test + CI foundation** (≤ 1 day, can run parallel to 0a once 0a's API is agreed):
- `Tests/` console app target `ReverseVerbTests` (`juce_add_console_app`, `juce::UnitTest`), linking the plugin sources (use a `ReverseVerbCore` static library target shared by the plugin and tests).
- Golden test: render a generated 0.2 s decaying-noise burst at 44.1/48/96 kHz with fixed settings; store checksum + RMS/peak per stage; tolerance ±1e-6.
- **Fuzz test:** 2000 iterations of random parameters (including extreme: trimStart=1, trim range <20 ms, tail 0.1 s and 8 s, sync on with BPM 20-300, pitch ±1 at 4 oct, vol 0), asserting: no NaN/Inf, `audio.getNumSamples() ≥ 1`, `hitIndex` within range or -1, peak ≤ 1.0 + ε, finite latency. Run in CI under **ASan+UBSan** (Linux, clang) — this would have caught the trim overrun fixed in PR #3.
- CI additions: `ctest` job; **pluginval** (strictness 5) on Linux + Windows against the built VST3; cache `build/_deps` to cut CI time.
- Required status checks on `master`: Windows, Linux, Tests, pluginval.

### 0.5 Branch and PR rules
- Trunk-based. One branch per feature: `feat/<id>-<slug>`. PR ≤ 400 changed lines where possible; split by "plumbing" then "UI".
- Rebase on `master` daily; squash-merge; delete branch.
- **No drive-by reformatting** (add `.clang-format` in PR-0a, never mass-reformat in feature PRs).
- Append-only edits in `createLayout()` and `IDs`; new UI components go in new files; only add 1-3 lines to `resized()` per feature.
- Each PR must include: spec ID, tests added, manual test notes, screenshots for UI, CI green, threading-rule self-check.

### 0.6 Definition of Done (every feature)
- Behaviour matches the spec's **Acceptance criteria**; unit/fuzz tests added or extended.
- Old project states load unchanged and sound identical (golden test with a saved v1 state file stored in `Tests/data/`).
- pluginval strictness 5 passes; ASan/UBSan clean; no new compiler warnings in `Source/`.
- Manual smoke in ≥ 2 DAWs (see 0.8); help text (`kHelpText`) and tooltips updated.
- README feature list + `CHANGELOG.md` updated.

### 0.7 Dependency graph and order
```
PR-0a refactor ──► PR-0b tests/CI ──┬─► F3 click-free + sync-load ─► F2 keytrack ─► F7 gain/limiter     (Stream A: audio, serial)
                                    ├─► F5 resizable UI ─► F1 presets ─► F6 undo/redo ─► F9/F10/F11     (Stream B: UI, serial)
                                    └─► F4 batch export (needs RenderEngine) ─► F8 browser                 (Stream C)
Later: F12 reverb engine, F13 IR, F14 multi-out, F15-F20 sound features, F21 themes, F22 MIDI drag
Release eng (R1-R4) can start any time after PR-0b, in parallel.
```
Within a stream, merge serially; across streams they touch disjoint files (Stream A: `Voices`, `RenderEngine`, `Params`; B: `ui/*`, new `PresetManager`, `UndoHistory`; C: new `BatchExporter`).

### 0.8 Manual DAW test matrix (per release)
FL Studio (primary), Ableton Live, Reaper, Bitwig (CLAP), Cubase or Studio One. Each: load project with plugin, play notes, automate a knob, save/reload project, offline bounce, change project sample rate/buffer, PDC on/off, rapid note retrigger.

### 0.9 Suggested release plan
- **v0.2 beta:** PR-0a/0b, F3, F1, F2.  **v0.3:** F5, F6, F7, F4.  **v0.4:** F8-F11.  **v1.0:** installers, signing, macOS, F12+ chosen subset.

### 0.10 Things needed before/while building (checklist)
- [ ] **JUCE + VST3 SDK licensing decision** (commercial JUCE licence vs open-source release). Verify the licence of the bundled VST3 SDK for the JUCE version you ship. Blocks any public/commercial distribution.
- [ ] Windows code-signing (OV/EV cert or Azure Trusted Signing); Apple Developer account (notarization); GitHub Actions secrets for both.
- [ ] A small CC0 test sample pack (kicks/snares/hats/claps, mixed sample rates 44.1/48/96, mono+stereo, 16/24-bit, one corrupt file) in `Tests/data/` for tests and demos.
- [ ] Factory preset sound design pass (~30 presets) by someone with ears — schedule once F1 lands.
- [ ] A designer pass (even informal) before F5/F21 so layout is decided once.
- [ ] pluginval binary in CI; `auval` on a macOS runner.
- [ ] Issue labels/templates, `CHANGELOG.md`, versioning policy (SemVer; bump `project(... VERSION)` per release, show in UI footer).
- [ ] Model/tool note: mechanical work (file moves, boilerplate tests, docs, installer scripts) is fine for a smaller/cheaper model; audio-thread code, state migration, and DSP should be written or reviewed by the strongest available model/human, because those bugs are silent.

---------------------------------------------------------------------------------------------------

## F3 — Click-free automation, smoothed gains, synchronous state load  (Stream A, do first)

**Problem (current code):** when a re-render completes while a voice is playing, `processBlock` reads the new buffer at the old position → click/jump. `dryParam`/`wetParam` are applied unsmoothed → zipper noise. After project load, `setStateInformation` only sets `dirty`; the buffer arrives later, so the first note (or an offline bounce start) can be silent.

**Goal:** no audible discontinuity when parameters change during playback; deterministic output for offline bounce; no silent first note after load.

**Spec**
1. *Gain smoothing:* `juce::SmoothedValue<float, Linear>` for Hit and Swell gains, 5 ms ramp, advanced per sample in `renderRange`. (`dry`/`wet` currently read once per block.)
2. *Voice crossfade on buffer swap:*
   - Add to `Voice`: `std::shared_ptr<const RenderedSample> fadeFrom; double fadePos; int fadeLen = 512 (≈10 ms @ 48k, scale with SR); int fadeRemaining; int fadeFromPos`.
   - In `processBlock`, keep `lastBuffer` (shared_ptr). If `r != lastBuffer && lastBuffer != nullptr`: for each active voice set `fadeFrom = lastBuffer`, `fadeFromPos = v.pos` and **map the new position preserving time-to-hit**: if both have `hitIndex ≥ 0`: `newPos = hitNew − (hitOld − posOld)` when `posOld < hitOld`, else `hitNew + (posOld − hitOld)`; clamp to `[0,total)`. If either has no hit: `newPos = clamp(posOld)`.
   - While `fadeRemaining > 0`: output = `old[fadeFromPos++]·(1−t) + new[pos++]·t`, `t` linear 0→1. Equal-power is unnecessary (correlated signals).
3. *Keep old buffers alive long enough:* extend `retired` from 2 to **8** entries and enforce a **minimum 20 ms between buffer swaps** on the render thread (sleep the remainder). 8 swaps × 20 ms = 160 ms ≫ 10 ms fade, so the audio thread can never hold the last reference.
4. *Synchronous render at the safe moments:*
   - `prepareToPlay`: if `rendered` is empty or sample rate changed and a source exists → `renderMutex`-locked `render()` (non-realtime context), clear `dirty`.
   - `setStateInformation`: after loading file + state → synchronous `render()` (block is acceptable on load; hosts call this off the audio thread). Clear `dirty` and `previewAfterRender`.
   - Offline mode: in `processBlock`, `if (isNonRealtime() && dirty.exchange(false)) render();` (blocking is acceptable offline). Makes bounces deterministic.
5. Do **not** crossfade when `r == lastBuffer`; zero overhead on the common path.

**Files:** `Voices.*`, `PluginProcessor.cpp/.h` (processBlock, prepareToPlay, setStateInformation, render swap), no UI.
**Edge cases:** voice finishing during fade; new buffer shorter than mapped pos (clamp → voice ends); trim removes the hit (`hitIndex=-1`) ; sample-rate change (buffers differ in rate → no fade, hard cut is correct); 8 voices all fading; `fadeFrom` pointing at a buffer whose size < `fadeFromPos` (treat as zero).
**Acceptance:** (a) Automating Size with a sine-sweep source while a note plays: max sample-to-sample delta across swap ≤ 2× the delta before the swap (test). (b) Project reload then immediate note → non-silent within the first block (test using direct `setStateInformation` then `processBlock`). (c) Offline bounce twice → bit-identical. (d) ASan clean; no allocation in `processBlock` (verify with a malloc-hook test or `juce::ScopedNoDenormals` + custom allocator check). (e) Golden test unchanged when nothing changes.
**Effort:** M (2 d). **Risk:** medium (audio-thread logic) → fuzz test must include "swap during playback".

---------------------------------------------------------------------------------------------------

## F2 — MIDI pitch tracking (keytrack)  (Stream A, after F3)

**Goal:** play the swell chromatically; one-shot behaviour unchanged when off.

**Parameters (version 2):**
| ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `keytrack` | bool | — | **false** | off = current behaviour |
| `rootNote` | AudioParameterInt | 0-127 | 60 | note that plays at original pitch. Display names `C-2…G8` (FL convention: 60 = C5; **choose one convention and document it**) |

**Playback spec**
- `rate = 2^((note − rootNote)/12)`, clamp `[0.25, 4.0]`. Voice stores `double pos`, `double rate`, `int startDelay`.
- `rate == 1` or keytrack off → existing integer fast path (must stay bit-identical).
- Otherwise 4-point cubic Hermite interpolation on both channels; guard indices at buffer ends (clamp, not wrap).
- Ending: voice ends when `pos ≥ total − 1`.
- **PDC alignment** (only when `align` on; latency `L = hitIndex` is reported for rate 1): the dry hit should still land on the note. Compute `d = L − hitIndex / rate`. If `d ≥ 0` → `startDelay = round(d)` samples of silence before the voice starts (rate>1: hit arrives sooner than L). If `d < 0` → start with `pos = −d · rate` (rate<1: skip into the swell). Applied at note-on inside the sample-accurate MIDI loop (add `startDelay` to the event's offset).
- Velocity unchanged (`getFloatVelocity`). Note-off ignored (one-shot). Retriggering the same note spawns a new voice (existing stealing rule: oldest).
- MIDI pitch bend: out of scope v1 (reserve: ±2 st, multiply into `rate` live; separate ticket).
- The Pitch sweep already baked into the buffer is unaffected (it composes).
- GUI "PITCH" readout while playing should show `bakedSemis + keytrackSemis` (extend `cached` lookup with `voice.rate`; needs `getPlayheadPosition()` → also publish active voice rate through an atomic).

**UI:** `KEYTRACK` toggle + root-note combo placed in the MIX group (pattern: `ToggleButton` + `ComboBox` with `ButtonAttachment`/`ComboBoxAttachment`; for an Int param use a combo populated 0-127 with note names and `ParameterAttachment`, or make `rootNote` a Choice of 128 entries). Tooltips + help text.
**Files:** `Params.*`, `Voices.*`, `PluginProcessor.cpp`, `ui/` (MIX group), help text.
**Acceptance:** note=root → output bit-identical to keytrack off. note=root+12 → duration halves ±1 sample, spectrum shifted +1 oct (test: FFT peak of sine source). note=root−12 with align on → dry hit lands within ±1 sample of `note-on + L` (test via impulse source). Fuzz includes random notes 0-127. No inf/NaN. CPU: 8 voices at rate≠1 < 3 % of one core at 48 kHz/64-sample blocks.
**Effort:** M (2-3 d). **Risk:** medium (alignment math) → unit-test the math separately.

---------------------------------------------------------------------------------------------------

## F7 — Output gain, soft limiter, peak meter  (Stream A, after F2)

**Parameters (v2):** `outGain` float −24…+12 dB, step 0.1, default 0 dB (smoothed 5 ms); `limiter` bool default **true**.

**DSP:** applied after voice mix, per channel, stateless and zero-latency:
```
y = x                      if |x| ≤ t            (t = 0.89 ≈ −1 dBFS)
y = sign(x)·(t + (c−t)·tanh((|x|−t)/(c−t)))      (c = 0.99 ceiling)
```
Continuous with continuous first derivative at `t`. This is a safety clip, not a mastering limiter; document that. No oversampling in v1 (state rationale: occurs rarely; revisit if users report aliasing).
**Export:** `exportWav` applies `outGain` then the same soft clip when `limiter` is on, so exported = heard.
**Meter:** `std::atomic<float> peakL, peakR` updated per block (max |y|, decayed in the GUI timer at 30 Hz, −60 dB floor); small stereo bar in the header with a red clip LED that latches until clicked.
**Files:** `Params.*`, `PluginProcessor.cpp`, `ui/` (meter component, header).
**Acceptance:** limiter off + gain 0 → bit-identical to current. Sine at +6 dBFS → output peak ≤ 0.99 and no discontinuity (derivative continuity test). Gain automation zipper-free. Meter reads within 0.1 dB of a test tone.
**Effort:** S (1 d). **Risk:** low.

---------------------------------------------------------------------------------------------------

## F5 — Resizable, HiDPI-aware window  (Stream B, do first in B)

**Goal:** resizable 75-150 % (and arbitrary within limits), crisp on HiDPI, size persists per project.

**Spec**
- Design coordinates stay 1060×720. Resize limits: min 795×540 (75 %), max 1590×1080 (150 %); fixed aspect ratio via `ComponentBoundsConstrainer::setFixedAspectRatio(1060.0/720.0)`; `setResizable(true, true)` (corner resizer).
- **[SPIKE] choose approach (½ day), then pick one:**
  - A (preferred, deterministic): introduce `RVContent` (a plain `Component`, 1060×720) that owns every current child and the existing `resized()` layout code unchanged; the editor holds `RVContent content` and in `resized()` does `content.setTransform(AffineTransform::scale(getWidth()/1060.0f)); content.setBounds(0,0,1060,720)`.
  - B: `AudioProcessorEditor::setScaleFactor()` on the editor. Verify behaviour in FL Studio VST3, Reaper, and the standalone; reject if the host also applies its own scaling (double scaling).
- Mouse hit-testing and tooltips come for free with transforms; verify dragging precision in `WaveformDisplay` (`plot()` uses local coordinates, fine).
- `WaveformDisplay` static image cache must re-render at the physical pixel size: already keyed on `getWidth()*scale`; add check using `g.getInternalContext().getPhysicalPixelScaleFactor()` (done) and invalidate when the transform changes (`parentSizeChanged`).
- Persist size: save `uiW`, `uiH` as properties on the saved state tree (not parameters); restore in editor ctor. Clamp restored values to limits and to the screen (`Desktop::getDisplays()`).
- Options menu gets "UI scale: 75 / 100 / 125 / 150 %" items that call `setSize`.
- Font sizes are in design units; no change needed under transform.
**Files:** `ui/ReverseVerbEditor.*` (restructure), `PluginProcessor.cpp` (two properties). **Do this before F1/F6 add more UI.**
**Acceptance:** at 75/100/150 % all text readable, no clipped/overlapping controls (screenshot checklist); drag handles hit accurately; resize while playing causes no audio glitches; reopen editor → previous size; Windows 125/150 % display scaling and a 4K monitor both look sharp (manual).
**Effort:** M (2 d incl. spike). **Risk:** medium (host scale interplay).

---------------------------------------------------------------------------------------------------

## F1 — Preset system  (Stream B, after F5)

**Goal:** save/load/browse presets; expose factory presets to the host's preset menu; dirty indicator; A/B compare.

**Data format:** UTF-8 XML file `*.rvpreset`:
```xml
<ReverseVerbPreset version="1" name="Big Hall Swell" category="Snare" author="..." >
  <PARAMS> ...apvts.copyState() children... </PARAMS>
</ReverseVerbPreset>
```
- Contains parameters only. **Does not contain the sample file path** (loading a preset must not change the loaded sample). Optionally excludes `trimStart/trimEnd` (checkbox "Include trim", default off, because trims are sample-specific).
- Locations: factory = embedded via `juce_add_binary_data(ReverseVerbPresets SOURCES Resources/presets/*.rvpreset)`; user = `File::userApplicationDataDirectory/<CircuitDriftLabs>/ReverseVerb/Presets/` (subfolders = categories).
**Class `PresetManager`** (new files, no dependency on UI): `refresh()`, `listFactory()`, `listUser()`, `load(const PresetRef&)`, `save(name, category, overwrite)`, `remove(name)`, `next()/prev()`, `isDirty()`, `currentName()`. Loading runs on the **message thread**: build `ValueTree` from XML, validate each PARAM id exists, clamp values into range, then for each parameter `beginChangeGesture/setValueNotifyingHost/endChangeGesture` (so hosts record automation and F6 undo captures one step). Unknown ids ignored; missing ids reset to defaults.
- Name rules: 1-64 chars; strip `\/:*?"<>|` and control characters; reject reserved Windows names (`CON`, `NUL`…); no leading/trailing dots/spaces. Overwrite requires confirmation. Factory presets are read-only ("Save As" prompts for a new user name).
- Dirty detection: compare current normalized values to the loaded preset with tolerance 1e-4; show `*` after name.
- Host program list: `getNumPrograms()` = factory count; `setCurrentProgram(i)` → load via `MessageManager::callAsync` (never on audio thread); `getProgramName(i)`.
- A/B: two in-memory snapshots; button toggles; "copy A→B". Persisted in state as `abSlot`.
- Current preset name stored in state property `presetName`.
**UI:** header strip left of the browser: `[◀] [name ▾ menu] [▶] [Save] [A|B]` — menu groups Factory / User, search field for >30 items. Keyboard: Up/Down in menu.
**Files:** new `PresetManager.*`, `ui/PresetBar.*`, `Resources/presets/`, `CMakeLists.txt` (binary data), `PluginProcessor` (program functions, `presetName`).
**Acceptance:** save → restart plugin → preset listed and loads identical normalized values (±1e-6); load never changes the sample; corrupt/partial file shows an error toast and changes nothing; invalid names rejected; 100 presets list in < 50 ms; host program change works in Reaper/FL; undo (F6) reverts a preset load in one step.
**Effort:** M (3 d) + sound-design pass for factory content. **Risk:** low-medium.

---------------------------------------------------------------------------------------------------

## F6 — Undo / redo  (Stream B, after F1)

**Design decision:** use a **snapshot history** owned by the processor, not `UndoManager` via APVTS, because several edits bypass attachments (`setParam()` from waveform handles, randomize, reset, preset load) and APVTS/`UndoManager` behaviour for host-originated changes is not guaranteed **[SPIKE only if you want to switch]**.

**Spec**
- `UndoHistory` (new): `std::vector<Snapshot>` (Snapshot = `std::array<float, N>` of all parameter normalized values + `trimStart/trimEnd`), cursor index, max depth 100.
- Capture rule: register as `AudioProcessorParameter::Listener` on every parameter. On `parameterGestureChanged(begin=true)` set `gestureActive`; on gesture end, or after 300 ms of no changes for non-gesture changes (automation/host), push a snapshot **if it differs** from the cursor snapshot by > 1e-5. Randomize / reset / preset load call `history.beginGroup()/endGroup()` so they are one step (`setParam` calls inside a group don't push).
- **Host automation playback must not flood history:** ignore changes while `isHostAutomating` — treat changes arriving with no GUI gesture and no group as automation; do **not** push those (undo is for user edits). Document this.
- `undo()`/`redo()` on the message thread: set `restoring = true`, apply snapshot with change gestures, `restoring = false`. Pushing a new snapshot truncates redo branch.
- Sample file changes are not part of undo in v1 (documented limitation).
- UI: header buttons ⟲ ⟳ (disabled state reflects availability), shortcuts Ctrl/Cmd+Z, Ctrl+Shift+Z / Ctrl+Y handled in `ReverseVerbEditor::keyPressed` (editor must `setWantsKeyboardFocus(true)`; note some DAWs steal these keys — also keep buttons).
- Clear history on `setStateInformation`.
**Files:** new `UndoHistory.*`, `ui/` header buttons, processor owns instance.
**Acceptance:** drag a knob (100 moves) = 1 undo step; randomize = 1 step; undo×N then redo×N restores exact values (±1e-6); history capped at 100; no push during restore (no infinite loop); undo while playing causes no glitch (F3 crossfade); memory < 100 KB.
**Effort:** M (2 d). **Risk:** medium (gesture edge cases) → unit test with a fake parameter set.

---------------------------------------------------------------------------------------------------

## F4 — Batch export (render a whole folder)  (Stream C)

**Prerequisite:** PR-0a `RenderEngine` pure function.

**Goal:** apply the current settings to every sample in the current folder and write WAVs, without disturbing live playback.

**UI:** `EXPORT ALL…` in the transport row (or Export button dropdown). Dialog:
- Output folder (default `<sampleFolder>/ReverseVerb Export`), file name pattern `{name}_reverse` (tokens `{name}`, `{index}`), collision policy: *Skip / Overwrite / Auto-number* (default Auto-number).
- Bit depth 16 / 24 (default) / 32-float; 16-bit gets TPDF dither (toggle, default on).
- Sample rate: *Host* (default) / *Match source* / 44.1 / 48 / 96 kHz.
- Normalize: Off (default) / peak to −1 dBFS / −0.1 dBFS.
- Scope: all files / selected range; includes subfolders (toggle, default off).
- Progress dialog with Cancel; summary dialog (ok / skipped / failed with reasons) and "Reveal in file manager".
**Implementation**
1. `BatchExporter : juce::Thread` takes an immutable `RenderSettings` snapshot (taken on the message thread at click time, including bpm, hit/swell gains, outGain/limiter from F7), a `StringArray` of files, and options.
2. Per file: `AudioFormatManager` read (cap 10 s as in `loadSampleFile`), `renderSample(settings, source, nullptr)` (no shared cache), apply dry/wet exactly as `exportWav`, optional normalize, optional SRC (`LagrangeInterpolator`/`CatmullRom` is fine offline; use `juce::dsp::Oversampling` only if quality complaints), write to `*.tmp` then atomically rename → cancel never leaves partial files.
3. Progress via `std::atomic<int>`; GUI polls with a timer (no `callAsync` flood). Cancel flag checked between files and every ~100 k samples inside long renders is unnecessary (render ≈ <1 s/file).
4. Thread safety: exporter touches **no** processor state after the snapshot. If the user changes parameters during export, results still match the snapshot.
5. Memory: one file at a time. Large folder (1000 files) must not accumulate memory (test).
**Files:** new `BatchExporter.*`, `ui/ExportDialog.*`, small edit in `resized()`.
**Acceptance:** 3-file folder with mixed rates → 3 WAVs, each identical (±1e-6) to single-file `exportWav` at the same settings; corrupt file listed as failed without aborting; cancel mid-run leaves no `.tmp`/partial files; export during playback causes no audible glitch; 500 files completes with flat memory; non-ASCII/long paths work on Windows.
**Effort:** M (3 d). **Risk:** low (isolated) — **highest value-per-effort for sample-pack makers.**

---------------------------------------------------------------------------------------------------

## Second-tier features (condensed specs)

**F8 Sample browser (M).** Popup panel listing the current folder (name, duration, rate); live search filter; click loads (existing `loadSampleFile`); star favourites and recent folders (10) persisted in a JSON `PropertiesFile` under app data (global, not per project). Alt-click auditions the *raw* hit only (un-reversed) through a small separate path in `processBlock` to avoid a re-render per hover. Keyboard: ↑/↓ to move, Enter to load, space to play. Acceptance: 2000-file folder lists in < 200 ms (scan on background thread, sorted); no UI stall.

**F9 Lockable randomize (S).** Lock toggles per group {Reverb, Swell, Pitch, Volume, Mix, Trim}; state property `locks` (bitmask). `randomizeReverb()` skips locked groups; add "Mutate" (random ±15 % of current values, clamped) next to Random. Wrap in an undo group (F6).

**F10 Intensity macro (S).** UI-only (no audio parameter): slider 0-100 % interpolating between two stored parameter vectors ("subtle" and "extreme") defined in code constants; applying writes real parameters inside one undo group. Deterministic, no hidden state. Tune the two endpoint vectors by ear.

**F11 Shortcuts + first-run help (S).** Space = play, ←/→ = prev/next sample, Ctrl+Z/Y undo/redo, Ctrl+S save preset, R = random, F1 = help. First open shows a dismissable 4-step overlay (load, tweak, trim, drag out); flag stored in global settings. Must not steal keys from the DAW when the editor lacks focus.

**F12 Reverb engine upgrade (L).** Add parameter `algo` (Choice, **append-only**, index 0 = "Classic" = current engine and the default for old states; 1 Room, 2 Hall, 3 Plate). New `FdnReverb` (8 or 16 delay lines, Householder feedback matrix, per-line one-pole damping, delay lengths mutually prime, two modulated lines with LFO 0.1-0.5 Hz ≤ 1 ms depth, decay from RT60: `g = 10^(−3·d/(RT60·sr))`). Same `setup/process` interface so the cache key just adds `algo`. Acceptance: measured RT60 within ±10 % of target across Decay settings; no DC; stable (no growth) at max decay; golden test for Classic unchanged; A/B listening test vs Classic. Risk: medium (tuning).

**F13 Impulse-response reverb (M-L).** `reverbSource` Choice {Algorithm, IR} + state property `irFile`. Implement own offline `FftConvolver` (uniform partitions 4096, `juce::dsp::FFT`) — do **not** use `juce::dsp::Convolution` (asynchronous, real-time-oriented). IR ≤ 10 s, energy-normalised, true-stereo handled by L/R convolution, resampled to host rate. Replaces `ReverbEngine::process` in render stage 3 only. Bundle 5-10 CC0 IRs; document licences. Acceptance: impulse in → IR out (±1e-5 after normalisation); 10 s IR renders < 1.5 s.

**F14 Separate outputs (M).** `BusesProperties` main + optional "Swell" and "Hit" stereo buses (disabled by default); `isBusesLayoutSupported` accepts main-only or main+extras; `renderRange` writes swell portion to bus 1 and hit to bus 2 when enabled, else sums to main. Standalone stays main-only. Verify in FL (multi-out routing), Reaper, Bitwig; pluginval layout tests.

**F15 Filter sweep across swell (M).** Params `sweepAmt` (−1…1, default 0 = off) and range fixed (200 Hz…20 kHz exponential); render stage 5b: `StateVariableTPTFilter` low-pass whose cutoff moves along the swell (time-varying, updated every 32 samples), swell region only; cache key + 1.

**F16 Saturation + stereo width on swell (S-M).** `drive` 0-1 (tanh waveshaper, 4× oversampled since offline), `swellWidth` 0-2 (mid/side). Render stage 6b. Defaults leave sound unchanged.

**F17 More sync options (S-M).** Append choices to `syncLen` (1/2 beat, 1/4, triplets, dotted…) — never reorder; change `kSyncBeats` to `double`; `RenderedSample::beats` → `double`; beat-line drawing generalised; read time signature from `getPlayHead()` for "bars"; raise `tail` max from 8 s to 20 s (guard memory: reject renders > 64 M samples).

**F18 Time-stretch pitch mode (L, spike first).** Option "Formant-preserving/stretch" using a permissively licensed library (e.g. Signalsmith Stretch, MIT — verify licence/version before adopting; avoid GPL libraries unless the open-source route is chosen). Spike: quality vs CPU on drum tails.

**F19 Humanize / round-robin (S-M, needs F2).** `humanize` 0-1: per note ±(gain 1.5 dB, cents 20, start offset ≤ 5 ms when `align` off) using a PRNG **seeded deterministically** (reset in `prepareToPlay`, seed from note counter) so bounces are reproducible.

**F20 Velocity mapping (S).** `velSens` 0-1 (1 = current linear gain), `velBalance` −1…1 (shifts level between swell and hit with velocity). *Correction to earlier brainstorm:* mapping velocity to swell **length** is not possible live (length requires a re-render); it would need pre-rendered velocity layers — deferred.

**F21 Themes (S-M).** Replace `RVColours` constants with a `Theme` struct and 4 palettes; global (not per-project) setting; live switch recolours `RVLookAndFeel`, invalidates waveform cache, repaints. Check text contrast ≥ 4.5:1.

**F22 Drag out MIDI (S).** Second pad writes a temp `.mid` (one note at `rootNote`, length = total) with `juce::MidiFile`, via `performExternalDragDropOfFiles`.

---------------------------------------------------------------------------------------------------

## Release engineering

**R1 CI hardening (S).** Matrix: Windows, Linux (Ubuntu 22.04 for older glibc), macOS-14. Cache `_deps`. Jobs: build, unit+fuzz (ASan/UBSan), pluginval, artifact upload. Pin `clap-juce-extensions` to a commit (currently `GIT_TAG main` — non-reproducible) and verify `JUCE 8.0.4` tag stays pinned.

**R2 macOS (M).** Universal binary (`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, deployment target ≥ 11.0), formats AU + VST3 + CLAP + Standalone (already enabled by CMake `APPLE` branch); `auval -v aumu Rvrb Shdv` must pass; hardened runtime + notarization (`notarytool`), `.pkg` via `pkgbuild/productbuild`.

**R3 Windows installer + signing (M).** Inno Setup script: VST3 → `%CommonProgramFiles%\VST3`, CLAP → `%CommonProgramFiles%\CLAP`, Standalone → Program Files, Start-menu shortcut, uninstaller. Sign binaries and installer (`signtool`/Azure Trusted Signing) in a tag-triggered `release.yml` using repository secrets; publish to GitHub Releases with SHA-256 checksums.

**R4 Linux packaging (S).** `tar.gz` + `install.sh` copying to `~/.vst3`, `~/.clap`; optional `.deb`. Build on Ubuntu 22.04 for glibc compatibility.

**R5 Docs/web (S).** User manual (generate from help text), changelog, demo audio, privacy statement (plugin collects nothing), support email/issue template.

---------------------------------------------------------------------------------------------------

## Appendix A — Test catalogue (quick reference)
| Test | Feature | Type |
|---|---|---|
| Golden render @44.1/48/96 kHz | all | unit |
| Param fuzz (extreme values) under ASan/UBSan | all | fuzz |
| Old-state load (v1 file in `Tests/data`) sounds identical | all | regression |
| Swap-during-playback max-delta | F3 | unit |
| State load then immediate note non-silent | F3 | unit |
| Offline bounce determinism | F3 | unit |
| keytrack bit-identity at root; octave duration/FFT; PDC landing | F2 | unit |
| Limiter continuity + export parity | F7 | unit |
| Preset round-trip, name validation, corrupt file | F1 | unit |
| Undo grouping, cap, no re-entrancy | F6 | unit |
| Batch ≡ single export; cancel leaves no partials; memory flat | F4 | integration |
| pluginval strictness 5 | all | CI |
| DAW matrix (0.8) | release | manual |

## Appendix B — Estimate summary (one developer)
PR-0a/0b 2 d · F3 2 d · F2 2-3 d · F7 1 d · F5 2 d · F1 3 d · F6 2 d · F4 3 d → **≈ 17-18 working days to v0.3**. Second-tier features ≈ 3-4 weeks. Release engineering ≈ 1 week plus waiting on certificates/accounts (start those early; lead time is days).
