# Developer Guide

## Build and tests

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ReverseVerbTests
build/ReverseVerbTests_artefacts/Release/ReverseVerbTests   # golden render, fuzz, voices, presets, undo, batch
```

- `RV_FUZZ_N=600` runs a longer parameter fuzz.
- `RV_UI_TESTS=1 RV_SNAPSHOT_DIR=<dir>` runs editor render tests and needs a display (`xvfb-run -a` on Linux).
- `RV_SANITIZE=ON` builds with AddressSanitizer and UBSan (disables LTO).
- The golden baseline is `Tests/data/golden.txt`. Regenerate deliberately with `RV_WRITE_GOLDEN=1`.
- CI runs the tests on Windows and Linux, plus sanitizers and pluginval.

JUCE is fetched by CMake `FetchContent` (JUCE 8.0.4 in `CMakeLists.txt`; the README says 8.0.7, so check which tag is in use).

## Where the tests are

`Tests/BatchTests.cpp`, `PresetTests.cpp`, `RenderTests.cpp`, `UndoTests.cpp`, `VoiceTests.cpp`, `UiTests.cpp`, shared helpers in `TestHelpers.h`.
