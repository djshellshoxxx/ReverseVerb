# ReverseVerb Help

Two plugins, one engine:

- **ReverseVerb** (instrument, VSTi): load or generate a hit, get a reverse-reverb swell that rises into it. Triggered by MIDI notes.
- **ReverseVerb FX** (effect): drop it on a drum track; every hit that passes the trigger gets a swell rendered and played *before* it, in real time, via plugin delay compensation.

Both windows have a `?` button with this text.

## Quick start (instrument)
1. Add ReverseVerb to the Channel Rack. LOAD a snare/hat/clap, drag a file onto the window, or press GENERATE.
2. `<` `>` step through every sample in that folder (auto-previews with your current settings).
3. Put notes in the piano roll. Turn on **Hit on note (PDC)** so the hit lands exactly on the note and the swell starts early.
4. EXPORT WAV or DRAG TO DAW to get the rendered sample.

## Quick start (FX)
1. Put ReverseVerb FX on a mixer insert for a drum track or bus.
2. Play. Watch the trigger scope: red ticks are triggers. Drag in the scope to set the threshold.
3. LISTEN solos the swells while you dial in. FREEZE locks one swell for every hit.
4. If the status bar says "missed", the swell is longer than the latency budget: shorten LENGTH or raise HOLD.

The FX cannot work on live input (mic, live playing) because the dry signal is delayed by the swell length. Use it on recorded or sequenced audio.

## Waveform
- Colour follows COLOR (violet = dark, cyan = bright) and turns red as BASS CUT rises. Orange = the dry hit.
- Click = audition. Drag on the wave: right = shorter, left = longer. Left edge = trim start.
- **Top strip** = trim window over the whole untrimmed material. Drag its edges or drag the window itself. Double-click to reset trim.
- **White line** = volume envelope. Drag the end dots (bottom = silence, top = 0 dB) and the middle square for tension. Double-click a dot to reset. Mouse wheel over the waveform nudges volume (over a dot: only that dot; Shift = fine).
- **Pink line** = pan envelope. Top = right, bottom = left. Same handles.
- Red lines = beats (bright = bar) when SYNC is on. Dashed line = where the dry hit begins.
- **Gate strip** (SYNC on): click a block to mute that beat, drag across blocks, right-click = all on.
- Bottom row: length, hit position, BPM, and live time / pitch / volume / pan while playing.

## Controls
**Reverb**: SIZE (room dimensions), DECAY (tail length), DAMP (high-frequency absorption), DIFFUSION (smooth vs grainy; the SPACE panel shows more faces), EARLY REF (first reflections), SEPARATION (left/right difference), WIDTH (stereo spread), DELAY (silence between swell end and hit).

**Swell**: LENGTH (tail seconds, disabled when SYNC is on), SHAPE (bend the envelope: negative = fuller early, positive = late rush), COLOR (low pass), BASS CUT (high pass), STRETCH (time-stretch the hit 1x to 32x into a long even body before the reverb; reverse mode = rise, forward mode = fall; the envelope is flattened automatically, SHAPE curves it).

**Mix + tone**: HIT / DRY level, SWELL level, BASS and TREBLE shelves on the final result, BOOST (FX only).

**Pitch**: PITCH sweeps from 0 at the start to the knob amount at the end. RANGE = 1/2/4 octaves. CURVE box: drag to change how fast the sweep happens. Pitch is applied after sync, so a pitch sweep changes the final length.

**Volume / Pan**: START, END, TENSION knobs mirror the lines on the waveform.

**FX (on the swell)**: Echo, Reverse Echo (echoes before the reverse, repeats lead into the hit), Chorus, Reverse Chorus. TIME = delay time or chorus rate, FEEDBACK (echo), DEPTH (chorus), MIX.

**Gate** (SYNC on): SHAPE Hard (on/off), Smooth (each beat fades in and out), LFO (smooth wave between on and off beats). DEPTH = how far muted beats drop. SHUFFLE plays the beats in random order, UNSHUFFLE restores, ALL ON un-mutes everything.

**Output**: NORMALIZE (peak to 0 dB), SWAP L/R, INVERT PHASE.

**Sync**: locks the total sample (swell + gap + hit) to 1/2/4/8 beats or 1 to 32 bars at the host tempo. Re-renders when the tempo changes. Hosts that don't report a tempo use 120 BPM.

**Mode** (instrument): Reverse Reverb (swell into hit), Forward Reverb (hit then tail = a fall), Dry (no reverb, just the hit with all edits).

**Generate** (instrument): synthesised closed hat, snare, clap, kick, rim.

**Trigger** (FX): THRESHOLD, SENSITIVITY (how much louder than the recent average a hit must be), HOLD (minimum time between triggers), HIT LENGTH (audio captured after the transient), GATE HIT (stop capturing early when the hit decays), TRIGGER Audio/MIDI/Both, MAX SWELLS (oldest fades out when exceeded), TRIGGER NOW, FOLLOW LEVEL (swell volume follows hit volume), FREEZE, LISTEN.

## Every knob
- Drag up/down. Shift for fine. Double-click resets to default.
- Right-click: **Set value...**, **Reset to default**, **Copy value**, **Paste value**, **DAW menu** (automation / MIDI learn when the host offers it), and a how-to-automate note.
- FL Studio: move the knob, then use the wrapper menu (top-left arrow) > "Create automation clip" for the last tweaked parameter, or "Browse parameters".

## Presets
Factory presets are built in. SAVE stores yours in `Documents\ReverseVerb\Instrument` and `Documents\ReverseVerb\FX` (the `...` button opens the folder). The combo shows "(custom)" once you change anything. Presets store settings, not the sample.

## Status readouts (FX)
"latency" = current PDC in seconds. "captured" = hits rendered. "active" = swells playing. "missed" = swell couldn't be rendered in time (raise HOLD / lower LENGTH). "dropped" = no free slot (lower MAX SWELLS or HOLD).
