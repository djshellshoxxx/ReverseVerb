# ReverseVerb 2.0: CI, Direction, Quantization, and Gator Design

Date: 2026-09-03
Status: Approved design, pending implementation plan
Target branch: `feature/v2-host-automation`

## Purpose

ReverseVerb 2.0 will add a Rise/Fall rendering engine, expanded musical quantization, and a tempo-synchronized gator. Every stage must compile and pass automated tests before the next stage begins. The work also establishes a reproducible local and GitHub-hosted build system so compiler, unit-test, and plugin-validation failures can be diagnosed from logs rather than inferred from source inspection.

## Scope and delivery order

The implementation is strictly ordered:

1. Build and test foundation.
2. Rise/Fall direction engine.
3. Quantization upgrade.
4. Tempo-synchronized gator.
5. Full regression, plugin validation, documentation, and release-readiness review.

Each numbered stage ends in a separate commit and a green verification checkpoint. A later stage may not begin while an earlier checkpoint is failing.

The work remains in the existing Version 2.0 draft pull request until all stages pass. It will not be merged into `master` automatically.

## Non-goals

- This work will not attempt to manipulate FL Studio Playlist objects or write Automation Clip points through a private API.
- It will not add licensing, copy protection, sample generation, sidechains, multiple outputs, or a new preset browser.
- It will not replace the existing reverb algorithm solely for stylistic reasons.
- It will not add a general-purpose modulation system beyond the gator controls required here.
- It will not expose every gate step as a host parameter; patterns are plugin state, while performance controls are automatable parameters.

## Engineering rules

- The audio thread performs no file I/O, heap allocation, blocking lock acquisition, message-thread calls, or parameter-tree mutation.
- Existing parameter IDs remain stable. New parameters use stable IDs and explicit defaults.
- Existing Version 1 project state must load without crashing and must preserve its audible Rise behavior.
- Host-facing parameter changes use begin/end gestures and `setValueNotifyingHost`.
- DSP calculations use sample positions or rational musical durations; UI strings never drive DSP behavior.
- Shared calculations have one implementation used by playback, waveform rendering, and WAV export.
- New public units have a single responsibility and deterministic unit tests.
- Third-party actions and validation binaries are pinned to reviewed versions; downloaded validator archives are checksum-verified.

## 1. Build and test foundation

### Repository additions

- `.github/workflows/ci.yml`
- `CMakePresets.json`
- `Tests/TestMain.cpp`
- `Tests/DirectionTests.cpp`
- `Tests/MusicalTimeTests.cpp`
- `Tests/GateEngineTests.cpp`
- `scripts/bootstrap-dev.sh`
- `scripts/bootstrap-dev.ps1`

The scripts install or locate project-local CMake and Ninja when they are absent, then configure using checked-in presets. They verify tool versions before building. They do not require modifying global compiler settings.

The test executable uses JUCE's unit-test framework to avoid adding a second C++ testing framework. CTest invokes the executable and propagates failures through its exit status.

### GitHub Actions matrix

| Runner | Build products | Tests |
|---|---|---|
| Windows | VST3, Standalone | Debug compile, Release compile, unit tests, pluginval VST3 |
| macOS | VST3, AU, Standalone | Debug compile, Release compile, unit tests, pluginval VST3/AU |
| Ubuntu | VST3, Standalone | Debug compile, Release compile, unit tests, pluginval VST3 under a virtual display |

The workflow runs for pull requests, pushes to Version 2.0 branches, manual dispatches, and pushes to `master`. Build artifacts, CTest logs, and pluginval logs are uploaded even when validation fails. Dependency and JUCE downloads are cached using keys derived from the operating system, compiler, CMake files, and the pinned JUCE revision.

The workflow uses bounded timeouts and cancels obsolete runs for the same branch. A successful baseline run against the current Version 2.0 code is required before feature implementation begins.

### Local/direct compilation

During an implementation session the repository is built directly in an isolated checkout using the same presets as CI. If the execution environment lacks CMake or Ninja, the bootstrap script installs pinned user-local copies. The available native compiler is used for fast edit/build/test loops; GitHub Actions supplies the other operating systems and compilers.

### Validation levels

1. Compiler warnings enabled through JUCE's recommended warning flags.
2. Unit tests through CTest.
3. Debug and Release builds.
4. pluginval headless validation at strictness level 5 initially.
5. Strictness raised only after level 5 is stable, so infrastructure problems are distinguishable from plugin defects.

## 2. Rise/Fall direction engine

### User behavior

A new automatable `direction` choice has two values:

- **Rise**: reversed wet reverb leads into the dry hit. This preserves the current sound and is the default for migrated Version 1 state.
- **Fall**: the dry hit occurs first and the forward wet reverb decays after it.

The existing Delay control becomes a direction-aware separation:

- In Rise mode it is the silence between the wet rise and dry hit.
- In Fall mode it is the silence between the dry hit and wet fall.

The Hit and Swell level controls remain independent in both modes. Trim, volume envelope, pitch sweep, audition, MIDI velocity, drag-to-DAW, and WAV export retain their existing meaning over the final timeline.

### Rendered data model

The current single combined buffer is replaced by aligned layers:

- `wetAudio`: the rise or fall reverb layer.
- `dryAudio`: the original hit layer.
- timeline metadata: total length, dry-hit sample, wet start/end, trim range, beat information, and display envelopes.

Playback and export use the same mix function:

`output = wetAudio * swellLevel + dryAudio * hitLevel`

Separate layers permit Fall mode to overlap the reverb tail with the dry hit without misclassifying samples as exclusively wet or dry. A lightweight display mix is generated off the audio thread for waveform drawing.

### Rendering sequence

1. Decode and resample the source hit.
2. Create the forward wet reverb response without duplicating the separately mixed dry hit.
3. Select orientation: reverse wet response for Rise; retain forward response for Fall.
4. Apply wet filtering and swell shaping.
5. Place wet and dry layers on the direction-specific timeline.
6. Apply direction-safe trim and the existing pitch/volume transformations consistently.
7. Apply short boundary fades where required to prevent discontinuities.
8. Publish one immutable rendered snapshot to the audio thread.

### Timing and latency

- Rise with Hit-on-note enabled reports the dry-hit offset as plugin latency.
- Rise with Hit-on-note disabled reports zero latency.
- Fall always reports zero lookahead latency because the dry hit begins at the trigger.
- Changing direction or alignment updates reported latency after the new rendered snapshot is ready.
- Note-on events remain sample accurate within the incoming audio block.

### UI

The direction selector sits near the waveform and uses explicit RISE and FALL labels. The waveform places the orange dry hit and wet waveform in their real timeline order. Help text explains the direction-aware Delay behavior and PDC difference.

### Direction acceptance tests

- Version 1 state loads in Rise mode.
- Rise places the wet layer before the dry-hit sample.
- Fall places the dry hit before the wet layer.
- Fall reports zero latency for both alignment-toggle states.
- Rise latency equals the rendered dry-hit offset when alignment is enabled.
- Hit and Swell gains affect only their respective layers in playback and export.
- Mono and stereo source files produce finite stereo output at supported sample rates.
- Empty, silent, very short, and maximum-length source buffers do not crash or produce NaN/Inf values.
- Repeated direction changes safely replace rendered snapshots while audio is processing.

## 3. Quantization upgrade

### Musical duration model

Musical durations are represented as rational quarter-note lengths rather than display-text indices. Straight, dotted, and triplet values are derived exactly before conversion to samples.

Supported choices:

- 1/64 triplet, straight, dotted
- 1/32 triplet, straight, dotted
- 1/16 triplet, straight, dotted
- 1/8 triplet, straight, dotted
- 1/4 triplet, straight, dotted
- 1/2 triplet, straight, dotted
- 1/1 and 2/1 straight
- 1 bar, 2 bars, 4 bars, and 8 bars

Bar durations use the host time-signature numerator and denominator. For example, one 7/8 bar is 3.5 quarter notes; it is not treated as four beats.

As in Version 1, sync length defines the total rendered timeline rather than only the reverb-decay portion. Source-hit duration and direction-aware Delay are accounted for when solving the wet-tail length.

### Host timing snapshot

`processBlock` reads BPM, time signature, PPQ position, play state, and loop information from `AudioPlayHead::PositionInfo` when present. Validated timing values are copied into an atomic or immutable snapshot for non-audio rendering work.

Fallbacks are deterministic:

- Missing or invalid BPM: 120 BPM.
- Missing or invalid time signature: 4/4.
- Missing PPQ: note-retrigger phase for the gator.

BPM is clamped to a defensible operating range before division. Sample counts use double-precision intermediate calculations and round to the nearest valid sample.

### State compatibility

The seven existing `syncLen` choices and their parameter ID remain functional for Version 1 projects and automation. Version 2 adds a new fine-division parameter rather than reordering the legacy choice list. Migrated Version 1 state remains in legacy compatibility mode until the user selects a Version 2 division; at that point the new division becomes authoritative. The compatibility-mode marker is saved in the plugin state.

### Quantization UI

The sync control presents divisions in musical order and visually distinguishes triplet (`T`) and dotted (`D`) choices. The waveform grid is derived from the same rational-duration functions used by DSP. Labels show the active time signature when host information is available.

### Quantization acceptance tests

- Every supported division produces the expected rational quarter-note length.
- Sample counts match expected values at 44.1, 48, 88.2, 96, and 192 kHz within one sample.
- Tests cover multiple BPM values and 3/4, 4/4, 5/4, 6/8, 7/8, and 12/8.
- Invalid BPM/time-signature input selects documented fallbacks without NaN, division-by-zero, or negative lengths.
- Version 1 `syncLen` states produce the same durations after migration.
- Existing legacy automation continues to affect duration while compatibility mode is active.
- UI grid lines and rendered boundaries use the same calculation results.

## 4. Tempo-synchronized gator

### Signal placement

The gator is a real-time gain processor applied at the layer-mixing stage, with a target choice of Swell, Hit, or Both. Swell is the default. The selected layer is gated before summing so an unselected layer remains unchanged. The same deterministic engine is available to offline WAV export and drag-to-DAW rendering.

### Pattern model

- 16 or 32 visible steps.
- Each step stores a normalized level from 0 to 1.
- Patterns are stored as a child `ValueTree` in plugin state, not as dozens of host parameters.
- Audio processing consumes an immutable pattern snapshot atomically exchanged from the message thread.
- Pattern length, rate, depth, smoothing, swing, phase, retrigger mode, target, and global gate shape are host-automatable parameters.

### Rates and synchronization

The step-rate selector supports straight, dotted, and triplet values from 1/4 through 1/64. Two phase modes are provided:

- **Note**: step zero begins at each triggered voice.
- **Host**: step position follows host PPQ so the pattern remains locked across playback and loop boundaries.

If host PPQ is unavailable, Host mode falls back to Note mode without interrupting audio.

### Gate shapes

The global shape selector provides:

- Square
- Smooth square
- Ramp up
- Ramp down
- Triangle
- Sine
- Curved

Shape evaluation is bounded to the current step. Smoothing applies a short sample-rate-aware transition that prevents clicks without audibly erasing fast patterns. Swing offsets alternating step boundaries while preserving the overall two-step duration.

### Pattern editor operations

- Click or drag to paint levels.
- Shift-drag draws a line across steps.
- Clear, fill, invert, randomize, rotate left, and rotate right.
- Copy and paste pattern data within ReverseVerb instances.
- Pattern changes are undoable on the message thread.
- Randomization uses an explicit stored seed when repeatability is required.

### Real-time processing

The GateEngine accepts a timing snapshot, voice-relative sample position, and immutable pattern. It returns per-sample or ramped gain without allocation or locks. Parameter discontinuities are smoothed. Host transport jumps and loop wraps recompute phase from PPQ rather than accumulating timing drift.

For Hit-only processing, the dry layer is gated before layer summing. For Swell-only processing, the wet layer is gated before layer summing. Both applies the same phase to both layers. This requires the separate direction-engine layers and is why the gator follows direction work.

### Gator acceptance tests

- Step lookup is exact at boundaries for all supported sample rates and BPMs.
- Straight, dotted, triplet, swing, phase-offset, and loop-wrap cases are covered.
- Every shape starts and ends within its documented range and never produces NaN/Inf values.
- Smoothing bounds sample-to-sample gain discontinuities.
- Note retrigger begins at step zero; Host mode follows supplied PPQ.
- Missing PPQ falls back deterministically.
- Target selection modifies only the selected layer.
- Pattern edits publish immutable snapshots without audio-thread locks or allocations.
- Saved state restores all steps, settings, and random seed.
- Offline export and live playback produce matching output when given the same phase origin.

## Error handling and limits

- Invalid or unreadable source files leave the last valid render intact and report failure to the UI.
- Render lengths are bounded before allocation to prevent unreasonable memory requests at extreme tempos or sample rates.
- A failed background render is not published to the audio thread.
- Unsupported hosts receive documented timing fallbacks.
- CI download failures are distinguished from compiler, unit-test, and pluginval failures in separate steps.
- Plugin state parsing validates child types, pattern lengths, choice ranges, and numeric finiteness before accepting values.

## Documentation and manual validation

README and built-in help will document Direction, fine divisions, gator controls, state compatibility, and latency behavior. The final checklist includes manual testing in FL Studio for:

- Parameter right-click host menu.
- Automation recording and playback.
- Rise PDC alignment.
- Fall zero-latency triggering.
- Tempo and time-signature changes.
- Playlist loop and transport jumps.
- Drag-to-DAW and exported WAV equivalence.

FL Studio project-object manipulation is outside the VST3 test boundary and therefore remains a manual host-integration test.

## Completion criteria

The Version 2.0 feature set is complete only when:

1. All unit tests pass locally and through CTest.
2. Debug and Release builds pass on Windows, macOS, and Ubuntu.
3. pluginval passes at the agreed strictness on VST3 and AU where applicable.
4. No compiler warnings introduced by the changed code remain unresolved.
5. Version 1 state migration tests pass.
6. README, built-in help, and parameter descriptions match behavior.
7. The final diff contains no unrelated refactoring or generated build products.
8. Manual FL Studio validation results are recorded in the pull request.
