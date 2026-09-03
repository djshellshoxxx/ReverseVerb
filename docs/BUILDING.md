# Building from source

## Windows (recommended path)
1. Keep the source at a short path, e.g. `C:\ReverseVerb`.
2. Right-click `build.ps1` > **Run with PowerShell**, as Administrator (needed to write into Program Files).
3. First run installs Git, CMake and the Visual Studio C++ Build Tools via winget, then downloads JUCE (~200 MB) and builds. Budget 15-30 minutes. Later runs take 1-3 minutes.

Manual equivalent:
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target ReverseVerb_VST3 ReverseVerbFX_VST3 ReverseVerb_Standalone ReverseVerbFX_Standalone
```

## macOS
Xcode command line tools (`xcode-select --install`), then:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Produces VST3 + AU + standalone and copies them into `~/Library/Audio/Plug-Ins`.

## Linux
```
sudo apt install build-essential cmake libasound2-dev libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libfontconfig1-dev libgl1-mesa-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(nproc)
```
VST3s land in `~/.vst3`.

## Troubleshooting the build
- **"Generator NMake Makefiles does not support platform specification"**: CMake picked the wrong generator. Delete `build\` and run `build.ps1` again (it forces the Visual Studio generator).
- **"CMAKE_CXX_COMPILER not set"**: the C++ workload isn't installed. Open Visual Studio Installer > Modify > tick "Desktop development with C++", then rerun.
- **PowerShell refuses to run the script**: `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned` once in an admin PowerShell.
- **JUCE download fails / hangs**: needs network access to github.com. Retry; FetchContent resumes. Behind a proxy set `HTTPS_PROXY`.
- **Build ran but nothing in Program Files**: run as Administrator (the copy step needs it), or copy `build\ReverseVerb_artefacts\Release\VST3\ReverseVerb.vst3` (and the FX one) there yourself.
- **Errors mentioning `juce_dsp` / SIMD on GCC**: this project deliberately doesn't link `juce_dsp`; if you add it, GCC 13 + JUCE 8.0.4 needs a newer JUCE tag.
- **Linker error about duplicate `createPluginFilter`**: you added a source file to both targets. Each plugin has its own processor; only `Source/Common` is shared.
- **Slow builds**: pass `-j` to `cmake --build` (Linux/mac) or set `CL=/MP` before building on Windows. The first build compiles JUCE once per target.

## Layout
```
Source/Common     Core.* (DSP, params, presets)  Widgets.* (look and feel, waveform, knobs)
Source/Instrument PluginProcessor.* PluginEditor.*
Source/Effect     FxProcessor.* FxEditor.*
docs/             HELP, INSTALL_TROUBLESHOOTING, BUILDING, CHECKLIST
```
Add features shared by both plugins in `Core`; parameters in `addReverbParams` / `addSwellParams` automatically appear in both.
