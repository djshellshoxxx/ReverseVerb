# ReverseVerb Windows-Only and Fast Startup Design

## Purpose

ReverseVerb 2.0 will temporarily support Windows only. At the same time, plugin startup and saved-project restoration will stop performing sample I/O, directory enumeration, DSP rendering, or construction of hidden editor pages on the host's message thread.

## Supported platform and formats

- Windows x64 is the only supported platform.
- Visual Studio 2022 is the supported generator and toolchain.
- VST3 and Standalone are the only built formats.
- AU, macOS, and Linux build, validation, bootstrap, packaging, and documentation paths are removed.
- CMake configuration on a non-Windows host must fail immediately with a clear Windows-only message.
- The existing Windows PowerShell bootstrap and one-shot build remain supported.

## Windows-only CI and release gate

The GitHub Actions workflow will contain one `windows-2022` job rather than a platform matrix. It will:

1. Check out the source and fetch pinned JUCE 8.0.4.
2. Configure and build Debug VST3, Standalone, and tests.
3. Run Debug CTest.
4. Configure and build Release VST3, Standalone, and tests.
5. Run Release CTest.
6. Download and SHA-256 verify pluginval 1.0.4 for Windows.
7. Validate the Release VST3 at strictness level 5.
8. Upload Windows Release artifacts, validation logs, and test logs.

The release gate requires this Windows job to be green plus completion of the existing FL Studio acceptance checklist.

## Startup performance boundaries

Startup is divided into four independently measurable phases:

1. Processor construction and parameter registration.
2. State parsing and parameter publication.
3. Saved-sample file decoding, sibling-folder enumeration, and DSP rendering.
4. Editor construction and first-page presentation.

Phases 1, 2, and the visible MAIN page must remain synchronous and fast. Phase 3 runs asynchronously. Hidden MOD, FX, and GATOR page controls are created only when their page is first selected.

## Asynchronous saved-sample restoration

`setStateInformation` parses and publishes APVTS, gate, envelope, MIDI mapping, preset, and saved-file metadata synchronously. It must not decode audio, enumerate a directory, or render DSP before returning.

When the state contains a saved sample path, the processor queues a background load request. The request:

- opens and decodes at most the same ten seconds of audio currently supported;
- enumerates and sorts supported sibling sample files away from the message thread;
- produces a source snapshot without mutating processor state from the worker;
- posts completion to the message thread for publication;
- schedules DSP rendering after successful publication.

An unreadable or missing saved sample leaves the processor operational with an empty rendered sample. The saved filename remains available for display so the user can identify the missing source. Failure must not block project loading or replace a newer user-selected/generated sample.

## Latest-request-wins cancellation

Every sample-load or render request receives a monotonically increasing generation number. Completion publishes only if its generation still matches the processor's current generation. Loading another file, generating a sample, restoring newer state, or destroying the processor invalidates older work.

The worker must not capture a raw processor pointer beyond processor lifetime. Shutdown cancels pending jobs and waits only for worker termination; no callback may access a destroyed processor or editor.

## Background rendering

Rendering consumes immutable snapshots of source audio, source sample rate, parameters, timing, gate pattern, and envelopes. It produces a complete `RenderedSample` privately and publishes the newest valid result atomically.

Parameter changes coalesce: while a render is running, further changes mark a newer generation instead of launching unbounded concurrent renders. When the current render finishes, only the newest requested snapshot needs to be rendered. Audio processing continues using the previously published immutable result until the replacement is complete.

Preview-after-load occurs only after the corresponding newest render is published. Latency updates are applied on the safe publication path.

## Lazy editor pages

The editor constructs the common header, transport, sample controls, preset row, tabs, MAIN page, and help plumbing immediately. MOD, FX, and GATOR page-specific controls and APVTS attachments are held in page-owned containers and constructed on first selection.

Host-visible parameters remain registered by the processor at all times; lazy UI construction does not alter parameter IDs, ordering, automation, serialization, or defaults. Selecting an uncreated page creates it once, synchronizes controls from APVTS, lays it out, and repaints. Repeated tab switches reuse the page.

## Compatibility requirements

- Version 1 state migration remains unchanged.
- Version 2 parameter IDs, ordering, defaults, gate patterns, envelopes, MIDI mappings, preset names, and saved sample paths remain compatible.
- Existing live/export DSP output must remain unchanged for the same source, state, host timing, and sample rate.
- User-initiated Load and Generate operations remain functional and use latest-request-wins semantics.
- No macOS or Linux compatibility promise remains in source configuration or documentation.

## Tests and evidence

Automated tests will verify:

- non-Windows CMake configuration is rejected with the intended message;
- state parsing returns without synchronous sample decoding or folder enumeration;
- a successful async restore publishes the expected source and rendered result;
- missing/unreadable files leave the instance usable;
- stale load and render completions cannot overwrite newer requests;
- processor destruction with work pending is safe;
- MAIN exists initially while MOD, FX, and GATOR are absent until selected;
- selecting each lazy page constructs it once and synchronizes APVTS state;
- legacy and current state round trips remain compatible;
- Debug and Release Windows unit tests pass;
- Windows VST3 and Standalone build;
- pluginval 1.0.4 validates the Release VST3 at strictness 5.

CI will record cold processor creation, state-return, and editor-first-paint measurements as artifacts. Assertions will target the synchronous units under test rather than total hosted-run duration, avoiding failures caused by shared-runner variability.

## Documentation

The README will describe Windows x64, VST3/Standalone, Visual Studio 2022, the PowerShell build paths, Windows-only automated verification, and the FL Studio release gate. Linux/macOS commands, AU claims, and cross-platform language are removed. Historical planning documents remain historical records and are not rewritten as current support documentation.
