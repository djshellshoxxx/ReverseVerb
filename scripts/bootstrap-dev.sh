#!/usr/bin/env sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tool_dir="$repo_dir/.tools/venv"
preset=${1:-debug}

if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
    python3 -m venv "$tool_dir"
    "$tool_dir/bin/python" -m pip install --disable-pip-version-check \
        "cmake==4.4.3" "ninja==1.13.2"
    PATH="$tool_dir/bin:$PATH"
    export PATH
fi

cmake --version
ninja --version
cmake --preset "$preset"
cmake --build --preset "build-$preset"
ctest --preset "test-$preset"
