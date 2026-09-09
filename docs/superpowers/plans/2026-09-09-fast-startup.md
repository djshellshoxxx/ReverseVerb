# ReverseVerb Fast Startup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Return quickly from plugin creation and state restoration by moving sample I/O/rendering off the message thread and constructing hidden editor pages only when opened.

**Architecture:** A processor-owned worker accepts immutable load/render requests tagged with generations and publishes only the latest result. State parsing stays synchronous; file decoding, folder enumeration, and DSP rendering are asynchronous. Page-owned editor groups lazily create MOD, FX, and GATOR controls without changing processor parameters.

**Tech Stack:** C++17, JUCE 8.0.4, APVTS, JUCE threading/message dispatch, CTest, pluginval 1.0.4.

**Spec:** `docs/superpowers/specs/2026-09-09-windows-only-fast-startup-design.md`

## Global Constraints

- Preserve Version 1 migration and every Version 2 parameter ID, order, default, and serialized value.
- Preserve live/export DSP output for identical input and timing.
- Worker completions use latest-request-wins generations.
- No callback may access a destroyed processor or editor.
- MAIN is immediate; MOD, FX, and GATOR initialize when first selected.

---

### Task 1: Extract immutable sample loading

**Files:**
- Create: `Source/SampleLoadService.h`
- Create: `Source/SampleLoadService.cpp`
- Create: `Tests/SampleLoadServiceTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `rv::SampleLoadResult loadSampleSnapshot(const juce::File&, juce::AudioFormatManager&)` with audio, sample rate, file, sorted folder files, current index, and error.
- Consumes: a registered format manager; reads at most ten seconds and supported sibling audio files.

- [ ] **Step 1: Write failing tests**

Create temporary WAV fixtures and assert decoding/sample rate, sorted siblings/index, ten-second truncation, and empty audio plus a nonempty error for a missing file.

- [ ] **Step 2: Run RED**

Build `ReverseVerbTests` and run Debug CTest. Expected: compilation fails because the service is absent.

- [ ] **Step 3: Implement the pure service**

Implement decoding, truncation, enumeration, sorting, and result construction without processor mutation or message-thread calls.

- [ ] **Step 4: Run GREEN and commit**

Run full Debug CTest and commit as `refactor: isolate sample loading from processor state`.

### Task 2: Restore saved samples asynchronously

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Create: `Tests/AsyncSampleRestoreTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `rv::SampleLoadResult` from Task 1.
- Produces: `requestSampleLoad(file, preview)` and a monotonically increasing sample-load generation.

- [ ] **Step 1: Write failing tests**

With an injectable loader and controlled executor, assert `setStateInformation` returns before loading runs, missing files remain usable, request 1 cannot overwrite request 2, Generate invalidates pending loads, and destruction prevents publication.

- [ ] **Step 2: Run RED**

Run Debug CTest. Expected: failures because state restoration still calls `loadSampleFile` inline.

- [ ] **Step 3: Implement minimal async loading**

Publish parameter/envelope/gate/MIDI state synchronously, queue the saved file, publish matching-generation results on a lifetime-safe path, and share the request path with user Load.

- [ ] **Step 4: Run GREEN and commit**

Run full Debug CTest and commit as `perf: restore saved samples off the message thread`.

### Task 3: Move DSP rendering to a coalescing worker

**Files:**
- Create: `Source/RenderRequest.h`
- Create: `Source/RenderWorker.h`
- Create: `Source/RenderWorker.cpp`
- Create: `Tests/RenderWorkerTests.cpp`
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: immutable `rv::RenderRequest`, `rv::RenderResult`, and `RenderWorker::request(RenderRequest)`.
- Consumes: source audio/rate, parameter snapshot, host timing, gate pattern, and envelopes.

- [ ] **Step 1: Write failing worker tests**

Assert no inline render, newest-request coalescing, stale-result rejection, continued access to the old rendered sample during work, preview only after newest publication, and safe shutdown.

- [ ] **Step 2: Run RED**

Run Debug CTest. Expected: failures because `timerCallback` still performs rendering.

- [ ] **Step 3: Implement snapshot rendering**

Move all render inputs into an immutable request and the algorithm into worker execution. Keep atomic `shared_ptr<const RenderedSample>` publication and safely publish latency/preview for the latest generation.

- [ ] **Step 4: Run GREEN and commit**

Run every Debug test, including DSP equivalence coverage, and commit as `perf: render immutable snapshots in the background`.

### Task 4: Lazily construct hidden editor pages

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Create: `Tests/LazyEditorPageTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: idempotent `ensurePageCreated(Page)` and page creation state exposed only under `JUCE_UNIT_TESTS`.
- Consumes: processor-owned APVTS parameters registered before editor creation.

- [ ] **Step 1: Write failing tests**

Assert MAIN exists initially, hidden pages do not, selecting each page constructs it exactly once, representative controls inherit prior APVTS values, and repeated switches create no duplicates.

- [ ] **Step 2: Run RED**

Run Debug CTest. Expected: failure because all page controls are currently eager.

- [ ] **Step 3: Implement page-owned lazy construction**

Keep common and MAIN controls immediate. Move hidden controls, attachments, tooltips, host bindings, and component lists into idempotent page creation methods called by `showPage`.

- [ ] **Step 4: Run GREEN and commit**

Run full Debug CTest and commit as `perf: create hidden editor pages on demand`.

### Task 5: Capture timing evidence and verify Release

**Files:**
- Create: `Tests/StartupTimingTests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Produces: named timings for processor construction, synchronous state return, editor construction/first paint, and async completion.
- Consumes: Tasks 2-4 async and lazy boundaries.

- [ ] **Step 1: Add behavioral timing tests**

Measure with JUCE high-resolution ticks. Assert state restoration returns before a controlled loader is released and initial editor construction excludes hidden pages. Log milliseconds without a fragile total-runner wall-clock threshold.

- [ ] **Step 2: Verify RED/GREEN**

Disable the async/lazy behavior to demonstrate assertion failure, restore it, and confirm the full Debug suite passes.

- [ ] **Step 3: Run Windows Debug and Release verification**

Build `ReverseVerbTests`, `ReverseVerb_VST3`, and `ReverseVerb_Standalone` with both Windows presets; run both CTest presets with output on failure.

- [ ] **Step 4: Upload timing logs and run pluginval**

Preserve CTest timing output in the Windows artifact, push the branch, and verify pluginval 1.0.4 strictness 5.

- [ ] **Step 5: Commit**

Commit timing coverage and CI artifact changes as `test: track ReverseVerb startup boundaries`.
