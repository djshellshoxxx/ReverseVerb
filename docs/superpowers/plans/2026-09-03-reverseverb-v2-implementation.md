# ReverseVerb 2.0 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a tested, cross-platform ReverseVerb 2.0 with Rise/Fall rendering, fine musical quantization, and a tempo-synchronized pattern gator.

**Architecture:** Preserve the JUCE APVTS parameter surface while separating dry and wet rendered layers. Add two small pure-DSP modules for musical duration conversion and gate evaluation, then integrate them through immutable render/pattern snapshots. CMake presets, JUCE unit tests, CTest, GitHub Actions, and pluginval provide sequential verification gates.

**Tech Stack:** C++17, JUCE 8.0.4, CMake 3.22+, Ninja/Visual Studio/Xcode, CTest, GitHub Actions, pluginval.

**Spec:** `docs/superpowers/specs/2026-09-03-reverseverb-v2-ci-direction-quantization-gator-design.md`

## Global Constraints

- Build VST3 and Standalone on Windows/Linux; build VST3, AU, and Standalone on macOS.
- Perform no allocation, file I/O, blocking locking, UI calls, or ValueTree mutation in `processBlock` or `renderRange`.
- Preserve every Version 1 parameter ID and default; Rise is the migration default.
- Use one duration implementation for rendering, waveform grids, and gate timing.
- Use one layer-mix/gate implementation for live playback and WAV export.
- Each task ends with tests and a focused commit; do not start the next task on a failing checkpoint.
- Do not merge the draft pull request automatically.

---

### Task 1: Reproducible build and unit-test runner

**Files:**
- Modify: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `Tests/TestMain.cpp`
- Create: `scripts/bootstrap-dev.sh`
- Create: `scripts/bootstrap-dev.ps1`

**Interfaces:**
- Produces: CMake target `ReverseVerbTests`, CTest test `ReverseVerbTests`, configure presets `debug` and `release`, build presets `build-debug` and `build-release`, test presets `test-debug` and `test-release`.

- [ ] Add `include(CTest)` and a `juce_add_console_app(ReverseVerbTests PRODUCT_NAME "ReverseVerb Tests")` target guarded by `BUILD_TESTING`.
- [ ] Add `Tests/TestMain.cpp` with a `main` that runs `juce::UnitTestRunner`, prints results, and returns nonzero when failures are present.
- [ ] Add presets using `Ninja Multi-Config`, `build/${presetName}`, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, and `COPY_PLUGIN_AFTER_BUILD=OFF`.
- [ ] Add bootstrap scripts that prefer existing CMake/Ninja, otherwise create `.tools/venv`, install pinned Python `cmake` and `ninja` packages, and execute the requested preset.
- [ ] Run the test executable before registering feature tests; expect zero suites and exit code zero.
- [ ] Build Debug and Release VST3/Standalone targets locally.
- [ ] Commit as `build: add reproducible CMake test foundation`.

### Task 2: Musical-time value module

**Files:**
- Create: `Source/MusicalTime.h`
- Create: `Source/MusicalTime.cpp`
- Create: `Tests/MusicalTimeTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `rv::TimeSignature { int numerator; int denominator; }`.
- Produces: `rv::Division` enum for all straight/dotted/triplet and bar choices.
- Produces: `double rv::quarterNotes(Division, TimeSignature) noexcept`.
- Produces: `int64_t rv::durationSamples(Division, double bpm, double sampleRate, TimeSignature) noexcept`.
- Produces: `rv::HostTiming rv::sanitiseTiming(double bpm, int numerator, int denominator, double ppq) noexcept`.

- [ ] Write table-driven tests for all division ratios.
- [ ] Run `ctest --preset test-debug`; expect new tests to fail because the module is absent.
- [ ] Implement rational quarter-note conversion, 120 BPM/4:4 fallbacks, finite-value validation, and rounded sample conversion.
- [ ] Add tests at 44.1/48/88.2/96/192 kHz and 3/4, 4/4, 5/4, 6/8, 7/8, 12/8.
- [ ] Run Debug/Release tests and `git diff --check`.
- [ ] Commit as `feat: add deterministic musical-time calculations`.

### Task 3: Direction-neutral rendered layers

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Create: `Tests/DirectionTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `enum class RenderDirection { rise, fall }`.
- Changes `RenderedSample` to aligned `wetAudio`, `dryAudio`, and `displayAudio` buffers plus `dryHitIndex`, `wetStart`, and `wetEnd`.
- Produces: `void mixRenderedRange(AudioBuffer<float>&, const RenderedSample&, int sourceStart, int destStart, int count, float wetGain, float dryGain)`.

- [ ] Write synthetic layer-mix tests proving Hit and Swell gains never affect the other layer and overlap sums correctly.
- [ ] Run tests and confirm failure before changing `RenderedSample`.
- [ ] Split current combined rendering into aligned wet/dry layers while retaining the existing Rise timeline and sound.
- [ ] Route `renderRange`, waveform display data, and `exportWav` through the shared layer mixer.
- [ ] Remove the duplicate synchronized `tailSec` assignment.
- [ ] Replace audio-thread `SpinLock::ScopedTryLockType` access with atomic shared-snapshot load/store supported by C++17 free functions.
- [ ] Run regression tests and syntax/build checks.
- [ ] Commit as `refactor: separate rendered wet and dry layers`.

### Task 4: Rise/Fall DSP and interface

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `Tests/DirectionTests.cpp`

**Interfaces:**
- Adds APVTS choice `IDs::direction` with `Rise` index 0 and `Fall` index 1.
- Produces: `RenderDirection ReverseVerbProcessor::getDirection() const noexcept`.
- Direction-aware Delay placement and latency reporting.

- [ ] Add failing tests for Rise wet-before-dry, Fall dry-before-wet, Rise PDC, and Fall zero latency.
- [ ] Add the stable `direction` parameter and parameter listener.
- [ ] Orient and place wet/dry layers per mode; remove duplicated direct transient from the wet path.
- [ ] Apply boundary fades and assert finite output for silence, one-sample input, mono, and stereo cases.
- [ ] Add a RISE/FALL selector near the waveform with an APVTS attachment and host context behavior.
- [ ] Update waveform hit marker, labels, Delay tooltip/help, and PDC enablement for Fall.
- [ ] Add state migration that selects Rise when the state lacks `direction` and writes `schemaVersion=2`.
- [ ] Run all tests, both local configurations, and diff checks.
- [ ] Commit as `feat: add rise and fall direction engine`.

### Task 5: Fine, time-signature-aware quantization

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `Tests/MusicalTimeTests.cpp`

**Interfaces:**
- Adds `IDs::syncDivisionV2` and state property `useV2SyncDivision`.
- Preserves `IDs::syncLen` for legacy state and automation.
- Produces: `rv::HostTiming ReverseVerbProcessor::getHostTiming() const noexcept`.

- [ ] Add failing migration tests mapping all seven legacy sync choices to identical durations.
- [ ] Capture sanitized BPM, time signature, PPQ, play, and loop data in `processBlock` without allocation.
- [ ] Add the fine-division parameter using display labels from `rv::Division` and switch authority only after a v2 selection.
- [ ] Calculate total render length from `durationSamples`, subtracting hit and direction-aware Delay with a 50 ms wet-tail minimum.
- [ ] Replace hard-coded four-beat bar/grid behavior with `rv::TimeSignature`.
- [ ] Add UI choices from 1/64T through 8 bars, and display the host time signature.
- [ ] Verify legacy automation remains effective in compatibility mode.
- [ ] Run all duration, migration, build, and diff checks.
- [ ] Commit as `feat: add fine host-aware quantization`.

### Task 6: Lock-free gate DSP core

**Files:**
- Create: `Source/GateEngine.h`
- Create: `Source/GateEngine.cpp`
- Create: `Tests/GateEngineTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `rv::GatePattern` with 32 fixed-capacity float steps, active step count, and seed.
- Produces: `rv::GateShape`, `rv::GateRetrigger`, and `rv::GateTarget` enums.
- Produces: `rv::GateSettings` containing rate, depth, smoothing milliseconds, swing, phase, retrigger, target, and shape.
- Produces: `float rv::GateEngine::gainAt(const GatePattern&, const GateSettings&, const HostTiming&, int64_t noteSample, int64_t hostSample) noexcept`.
- Produces: smoothed block application with no allocation or locks.

- [ ] Write failing tests for boundary lookup, shape ranges, depth, swing pair duration, phase, Note/Host retrigger, missing PPQ fallback, and finite output.
- [ ] Implement fixed-capacity pattern validation and step-phase calculation using `MusicalTime`.
- [ ] Implement Square, Smooth Square, Ramp Up, Ramp Down, Triangle, Sine, and Curved evaluators.
- [ ] Implement sample-rate-aware gain smoothing and transport-jump phase recalculation.
- [ ] Add a test-only allocation guard around block processing.
- [ ] Run Debug/Release tests and sanitizer-compatible Linux build flags where available.
- [ ] Commit as `feat: add lock-free tempo-synced gate engine`.

### Task 7: Gator state and automatable integration

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Tests/GateEngineTests.cpp`

**Interfaces:**
- Adds global parameters `gateEnabled`, `gateSteps`, `gateRate`, `gateDepth`, `gateSmooth`, `gateSwing`, `gatePhase`, `gateRetrigger`, `gateTarget`, and `gateShape`.
- Produces: `std::shared_ptr<const rv::GatePattern> getGatePattern() const`.
- Produces message-thread methods `setGateStep`, `replaceGatePattern`, `clearGatePattern`, `fillGatePattern`, `invertGatePattern`, `randomizeGatePattern`, and `rotateGatePattern`.

- [ ] Add failing state round-trip tests for all 32 steps, active length, settings, and random seed.
- [ ] Add global APVTS parameters with explicit ranges/defaults and cached atomic pointers for audio use.
- [ ] Store pattern data in a validated `GATOR_PATTERN` child ValueTree and publish immutable snapshots atomically.
- [ ] Apply the gate to wet, dry, or both layers before summing in live playback.
- [ ] Use the same gate path for offline export with note-origin phase.
- [ ] Verify parameter automation, state restore, target isolation, and live/export equivalence.
- [ ] Commit as `feat: integrate automatable gator processing`.

### Task 8: Gator editor and operations

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

**Interfaces:**
- Produces `GatePatternEditor` component bound to the processor's message-thread pattern API.
- Produces controls for enabled, steps, rate, depth, smooth, swing, phase, retrigger, target, and shape.

- [ ] Add the pattern editor with click/drag paint and Shift-drag line drawing.
- [ ] Add Clear, Fill, Invert, Randomize, Rotate Left, Rotate Right, Copy, and Paste operations.
- [ ] Use `juce::UndoManager` transactions for edits and validate clipboard pattern text before applying it.
- [ ] Add compact layout without overlapping the existing controls; maintain minimum control hit sizes.
- [ ] Add help text and accessible names/tooltips.
- [ ] Build the editor in Standalone and exercise resize/open/close repeatedly.
- [ ] Commit as `feat: add gator pattern editor`.

### Task 9: Continuous integration and plugin validation

**Files:**
- Create: `.github/workflows/ci.yml`
- Modify: `README.md`

**Interfaces:**
- Produces required jobs `build-windows`, `build-macos`, and `build-linux`.
- Produces downloadable plugin/test/validator-log artifacts.

- [ ] Add PR, `master`, `v2*`, and manual triggers with concurrency cancellation and bounded job timeouts.
- [ ] Install Linux JUCE build packages; use the checked-in presets on every runner.
- [ ] Cache JUCE/CMake downloads without caching generated plugin binaries.
- [ ] Build Debug/Release, run CTest, and upload logs on failure.
- [ ] Download pinned pluginval binaries, verify SHA-256, validate VST3 on all runners and AU on macOS at strictness 5.
- [ ] Upload Release artifacts with deterministic names.
- [ ] Push the checkpoint, inspect every job log, and fix failures before continuing.
- [ ] Commit as `ci: build and validate plugins on three platforms`.

### Task 10: Final regression and handoff

**Files:**
- Modify: `README.md`
- Modify: `Source/PluginEditor.cpp`
- Create: `docs/v2-fl-studio-test-checklist.md`

**Interfaces:**
- Produces user documentation and the manual host-integration checklist.

- [ ] Document Direction, Delay semantics, sync divisions, gator shapes/operations, PDC, and state compatibility.
- [ ] Run all local Debug/Release unit tests and builds from a clean build directory.
- [ ] Run `git diff --check`, inspect repository status, and confirm no generated files are tracked.
- [ ] Push and wait for all three CI jobs plus pluginval to pass.
- [ ] Review the complete diff for real-time safety, parameter-ID stability, bounds, lifetime, and state migration.
- [ ] Record the remaining FL Studio manual checks without claiming they were automated.
- [ ] Commit as `docs: complete ReverseVerb 2.0 feature documentation`.
