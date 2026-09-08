#!/usr/bin/env bash
# Lightweight static quality gate for AETHR CI / local use.
# High signal, low noise — fails only on clear repository hygiene problems.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"

fail=0

say() { printf '%s\n' "$*"; }
bad() { say "FAIL: $*"; fail=1; }

say "== Static checks =="

# Tracked junk that should never ship
while IFS= read -r path; do
    bad "tracked junk: ${path}"
done < <(git ls-files | grep -E '(^|/)\.DS_Store$|\.o$|\.a$|\.dylib$|/build/|External/JUCE/|External/Catch2/|pluginval\.app/' || true)

# Forbidden branding in user-facing / source trees
if git grep -n -I -E 'BrainWavez|Brainwavez|brainwavez' -- \
    'Source' 'Tests' 'docs' 'README.md' 'CMakeLists.txt' 'CMakePresets.json' \
    ':(exclude)docs/CI.md' 2>/dev/null | grep -v '^Binary'; then
    bad "forbidden BrainWavez branding found in tracked sources"
fi

# Accidental iCloud conflict copies
while IFS= read -r path; do
    bad "iCloud conflict copy tracked: ${path}"
done < <(git ls-files | grep -E ' 2(\.|$)' || true)

# Parameter ID rename guard: IDs must stay lowercase dotted
if ! grep -q 'engine\.type' Source/Parameters/ParameterIDs.h; then
    bad "engine.type missing from ParameterIDs.h"
fi

say "Static checks complete."
exit "${fail}"
