# Feature checklist

## Originally requested (done)
- [x] Load / browse folder / preview with settings; drag file in; DRAG TO DAW; EXPORT WAV
- [x] Reverse reverb with size, damp, delay, decay, diffusion, early reflections, separation, width, color, shape, bass cut
- [x] Waveform with live playhead, depth, colour driven by COLOR and BASS CUT, SPACE wireframe panel
- [x] Click waveform to play; drag to trim; overview trim window; reset edits
- [x] Sync 1/2/4/8 beats and 1 to 32 bars with beat lines; gate per beat with Hard / Smooth / LFO shapes and depth; shuffle beats
- [x] Pitch sweep 1/2/4 octaves with tension curve box
- [x] Volume line with two points and tension (FL automation style); pan line; mouse wheel volume
- [x] Live readouts (time, pitch, volume, pan, length, hit position, BPM)
- [x] Help button, tooltips, right-click knob menu (set value, reset, copy/paste, DAW automation)
- [x] Generate hat / snare / clap / kick / rim; Reverse / Forward / Dry modes (unreverse, dereverb)
- [x] Echo, reverse echo, chorus, reverse chorus on the swell
- [x] Normalize, swap stereo, invert phase, bass and treble boost
- [x] Stretch (granular time-stretch with envelope smoothing) for smooth rises / falls
- [x] VST effect version with continuous transient capture, PDC alignment, freeze, listen, follow level, boost, max swells, trigger scope
- [x] Presets (factory + user) for both plugins
- [x] Docs: help, install troubleshooting, building from source

## Review fixes (done this session)
- [x] Voices keep the render they started with; renders freed only on the message thread
- [x] FX Freeze state cleared on prepareToPlay; freeze latency follows the frozen buffer
- [x] Bounded pitch loop (no 60 s over-generation)
- [x] Safe WAV export (temp + rename, all results checked); unique drag-out temp file with SafePointer
- [x] Partial-hit trim keeps the remaining hit flagged as hit
- [x] Trigger hysteresis / re-arm; voice stealing fades instead of cutting
- [x] Parameter gestures begin/end once per drag on waveform and curve controls
- [x] Preset "custom" marking; user preset name collision with factory names
- [x] getTailLengthSeconds reports real tail; soft clip on summed output; Listen mutes audition hit; missing sample clears the old source

## Review items not done yet
- [ ] Instrument render on a worker thread (currently message thread; fine up to a few seconds of swell)
- [ ] Shared renderer for instrument and FX (duplicated pipeline)
- [ ] Sample path relink / embedded sample
- [ ] Smoothing for dry/wet automation
- [ ] Units on every value display, resizable / scalable GUI
- [ ] Undo / redo, A/B, parameter locks for Random
- [ ] Separate pitch target swell / hit / both

## Suggested additions (from the feature list) - not started
- [ ] Trigger filter (low/high cut) + listen-to-trigger, external sidechain, trigger profiles
- [ ] Dotted / triplet sync, host time signature, swing
- [ ] Multi-point envelopes
- [ ] Multiple reverb algorithms, modulation, shimmer, convolution IR mode
- [ ] Texture section (saturation, bit crush, noise)
- [ ] Multi-sample slots, MIDI behaviour controls, sample browser
- [ ] Frequency-band swells, trigger sequencer, batch export, stem export, render variations
- [ ] Simple / advanced GUI modes, meters, clip indicator
