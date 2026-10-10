# Changelog

## Unreleased (beta)
### Added
- Presets (factory + user), A/B compare, dirty marker
- Keytrack with root note; hit stays on the note with PDC
- Output gain, soft limiter, peak meter + clip light
- Undo / redo
- Batch export of a whole folder
- Resizable window, remembered per project
- CLAP format; Windows + Linux CI builds, test suite, pluginval, sanitizer run
### Changed
- Rendering runs on a background thread; reverb stage is cached so trim / pitch / volume edits are instant
- Parameter changes crossfade during playback (no clicks); gain changes are smoothed
- Projects render immediately on load; offline bounces are deterministic
- Consistent knob value boxes
### Fixed
- Out-of-bounds read when the trim start was dragged to the very end
- Unstable filters (loud garbage) when Color / Bass Cut were above Nyquist at low host sample rates (e.g. 22.05 kHz)
