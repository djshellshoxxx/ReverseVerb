# ReverseVerb 2.0 — FL Studio acceptance checklist

Automated unit tests and pluginval cover DSP, state parsing, editor creation, and plugin-format behavior. The items below require a real FL Studio session and should be completed before calling a build release-ready.

Record the FL Studio version, operating system, audio-device buffer size, ReverseVerb commit, and plugin format with the test results.

## Installation and lifecycle

- [ ] Scan `ReverseVerb.vst3` in **Options > Manage plugins** with no errors or duplicate entries.
- [ ] Add it to the Channel Rack as an instrument and confirm stereo output.
- [ ] Open, close, and resize the editor at least ten times; verify no clipped controls, overlaps, hangs, or crashes.
- [ ] Save, close, and reopen the FLP; confirm sample path, all parameters, gator pattern, active step count, and random seed return unchanged.
- [ ] Load a Version 1 project/state and confirm Rise is selected and every legacy sync length sounds at its original 4/4 duration.

## Rise/Fall and PDC

- [ ] In Rise, verify the wet swell precedes the dry hit and Delay creates space before the hit.
- [ ] Enable **Hit on note (PDC)** in Rise and verify the dry transient lands on the piano-roll note against a metronome and a phase-aligned reference click.
- [ ] Disable PDC and verify playback begins immediately without lookahead compensation.
- [ ] Switch to Fall and verify the hit is first, the forward reverb begins after Delay, PDC is disabled, and FL Studio reports zero plugin latency.
- [ ] Automate Direction between patterns and confirm changes are stable and do not corrupt saved state.

## Musical quantization

- [ ] Test straight, triplet, and dotted values at 60, 120, and 174 BPM.
- [ ] Test one-, two-, four-, and eight-bar values in 3/4, 4/4, 5/4, 6/8, 7/8, and 12/8 projects.
- [ ] Confirm the rendered endpoint and waveform bar markers follow the current meter.
- [ ] Automate tempo during playback and confirm ReverseVerb re-renders without a crash or stale length.
- [ ] Verify the intentional 50 ms minimum wet tail when a chosen grid length is shorter than the hit plus direction/Delay requirements.

## Gator timing and sound

- [ ] Toggle 16 and 32 steps and paint discrete step levels by clicking and dragging.
- [ ] Hold Shift and drag across the editor; confirm it draws a continuous line between endpoints.
- [ ] Audition every Rate from 1/64T through 1/4D at multiple BPM values.
- [ ] Audition Square, Smooth, Ramp Up, Ramp Down, Triangle, Sine, and Curved shapes.
- [ ] Verify Depth reaches true bypass at 0 and full gating at 1.
- [ ] Raise Smooth until clicks disappear, then automate it and listen for discontinuities.
- [ ] Verify Swing alternates long/short step timing and Phase moves the pattern without changing its length.
- [ ] In Note retrigger, confirm overlapping MIDI notes each start at step 1.
- [ ] In Host retrigger, confirm multiple notes share the FL Studio song-position phase and remain aligned after start, stop, seek, and loop-wrap operations.
- [ ] Test Swell, Hit, and Both targets independently, especially where Fall wet/dry layers overlap.

## Pattern operations and automation

- [ ] Test Clear, Fill, Invert, Randomize, rotate left, rotate right, Undo, and Redo in both step modes.
- [ ] Copy a pattern, change it, then Paste; confirm all 32 stored values, active length, and seed return.
- [ ] Paste unrelated or malformed clipboard text and verify it is ignored without changing the pattern.
- [ ] Right-click every gator knob/combo/toggle and confirm **Create automation clip** appears when supplied by the installed FL Studio/VST3 host integration.
- [ ] Create automation clips for Enabled, Rate, Depth, Smooth, Swing, Phase, Retrigger, Target, and Shape; save/reload and verify links remain intact.
- [ ] Confirm no claim is made for FL stock-plugin-only actions such as Chop when the host does not expose them to third-party VST3 plug-ins.

## Live/export equivalence and stress

- [ ] Export the same preset used for live Note-retrigger playback, align both renders in the Playlist, invert one, and confirm they null apart from expected host gain/pan differences.
- [ ] Repeat the null check with Swell, Hit, and Both targets, nonzero Smooth, Swing, and Phase.
- [ ] Trigger at least eight overlapping notes at velocities 1 and 127; confirm deterministic voice stealing and no stuck voices.
- [ ] Test 44.1, 48, 88.2, 96, and 192 kHz where the audio device supports them.
- [ ] Test FL Studio buffer sizes from the smallest stable value through 2048 samples.
- [ ] Run for ten minutes while automating gator controls, tempo, Direction, Delay, Wet, and Dry; monitor for clicks, NaNs, CPU spikes, memory growth, or UI stalls.
