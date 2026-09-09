# ReverseVerb Windows-Only Build Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make ReverseVerb Windows x64-only with VST3 and Standalone outputs and one Windows CI release gate.

**Architecture:** CMake rejects non-Windows hosts before dependency setup. Windows presets and PowerShell entry points remain; generic Ninja, Unix bootstrap, AU, Linux, and macOS paths are removed. CI becomes one explicit Windows job.

**Tech Stack:** CMake 3.22+, Visual Studio 2022, PowerShell, JUCE 8.0.4, GitHub Actions, pluginval 1.0.4.

**Spec:** `docs/superpowers/specs/2026-09-09-windows-only-fast-startup-design.md`

## Global Constraints

- Windows x64 is the only supported platform.
- Visual Studio 2022 is the supported generator.
- VST3 and Standalone are the only formats.
- Preserve DSP behavior, state IDs, and Version 1 migration.
- Release requires green Windows CI plus the FL Studio checklist.

---

### Task 1: Enforce Windows-only configuration

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `CMakePresets.json`
- Remove: `scripts/bootstrap-dev.sh`
- Create: `scripts/test-windows-only-config.cmake`

**Interfaces:**
- Consumes: CMake `WIN32` and the `windows` configure preset.
- Produces: an immediate non-Windows failure and Windows-only presets.

- [ ] **Step 1: Write the failing policy test**

Create a CMake script that reads `CMakeLists.txt` and `CMakePresets.json`; require `if(NOT WIN32)`, `ReverseVerb currently supports Windows only`, and `FORMATS VST3 Standalone`; reject `AU`, `ninja-base`, and generic `debug`/`release` configure presets.

- [ ] **Step 2: Run RED**

Run `cmake -P scripts/test-windows-only-config.cmake`. Expected: FAIL because cross-platform configuration remains.

- [ ] **Step 3: Implement minimal production changes**

Add after `project(...)`:

```cmake
if(NOT WIN32)
    message(FATAL_ERROR "ReverseVerb currently supports Windows only")
endif()
```

Set `FORMATS VST3 Standalone`. Remove generic Ninja configure/build/test presets and delete `scripts/bootstrap-dev.sh`.

- [ ] **Step 4: Run GREEN**

Run `cmake -P scripts/test-windows-only-config.cmake`, followed on Windows by `cmake --list-presets`, `cmake --list-presets=build`, and `cmake --list-presets=test`.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt CMakePresets.json scripts
git commit -m "build: enforce Windows-only targets"
```

### Task 2: Replace the CI matrix

**Files:**
- Modify: `.github/workflows/ci.yml`
- Create: `scripts/test-windows-only-ci.cmake`

**Interfaces:**
- Consumes: Windows presets and targets from Task 1.
- Produces: one `build-windows` job on `windows-2022`.

- [ ] **Step 1: Write and run a failing workflow-policy test**

Reject `ubuntu`, `macos`, `matrix`, Linux/macOS validation, `ReverseVerb_AU`, and non-Windows artifact paths. Require all Windows presets, `pluginval_Windows.zip`, its pinned SHA-256, and `Validate Windows VST3`. Run the script and observe it fail against the current matrix.

- [ ] **Step 2: Implement the Windows job**

Keep checkout, JUCE cache/fetch, Debug and Release builds/tests, Windows pluginval checksum/strictness-5 validation, and Windows artifact upload. Remove all matrix indirection and non-Windows steps.

- [ ] **Step 3: Run GREEN and commit**

Run `cmake -P scripts/test-windows-only-ci.cmake`, then commit `.github/workflows/ci.yml` and the test as `ci: validate ReverseVerb on Windows only`.

### Task 3: Correct current documentation

**Files:**
- Modify: `README.md`
- Modify: `docs/v2-fl-studio-test-checklist.md`
- Modify: `scripts/test-windows-only-config.cmake`

**Interfaces:**
- Consumes: Tasks 1-2 commands and outputs.
- Produces: Windows-only setup and release-gate documentation.

- [ ] **Step 1: Extend the policy test and observe RED**

Require `Windows x64`, `VST3 and Standalone`, and both Windows build presets in README. Reject current-support `Linux/macOS`, `ReverseVerb_AU`, and `Windows/Linux` language.

- [ ] **Step 2: Rewrite documentation**

Keep only PowerShell commands, describe the one Windows CI job, and identify green Windows CI plus the FL Studio checklist as the release gate. Add Windows to checklist test metadata.

- [ ] **Step 3: Verify and commit**

Run both policy scripts and `git diff --check`; commit as `docs: mark ReverseVerb 2.0 Windows-only`.

### Task 4: Verify the Windows gate

**Files:** Verify all Task 1-3 changes.

**Interfaces:**
- Consumes: Windows-only project configuration.
- Produces: local policy evidence and one completed GitHub Actions run.

- [ ] **Step 1: Run both local policy scripts and `git diff --check`**
- [ ] **Step 2: Push `feature/v2-implementation` and inspect the sole Windows job**
- [ ] **Step 3: Confirm Debug/Release CTest, pluginval strictness 5, and artifact upload**
- [ ] **Step 4: Record the run URL and exact conclusions in the handoff**
