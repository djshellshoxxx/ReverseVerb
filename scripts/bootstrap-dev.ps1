param(
    [ValidateSet("debug", "release")]
    [string]$Preset = "debug"
)

$ErrorActionPreference = "Stop"
$RepoDir = Split-Path -Parent $PSScriptRoot
$ToolDir = Join-Path $RepoDir ".tools\venv"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue) -or
    -not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    python -m venv $ToolDir
    & "$ToolDir\Scripts\python.exe" -m pip install --disable-pip-version-check `
        "cmake==4.4.3" "ninja==1.13.2"
    $env:Path = "$ToolDir\Scripts;$env:Path"
}

cmake --version
ninja --version
cmake --preset windows
cmake --build --preset "build-windows-$Preset"
ctest --preset "test-windows-$Preset"
