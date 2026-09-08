#!/usr/bin/env bash
# Verify expected AETHR plugin artefacts exist after a CI / local build.
# Usage: Tools/ci-verify-artefacts.sh <artefacts-root> <os>
#   artefacts-root e.g. build/ci/Aethr_artefacts/RelWithDebInfo
#   os: macOS | Windows
set -euo pipefail

ART="${1:?artefacts root required}"
OS="${2:?os required (macOS|Windows)}"

fail=0
need() {
    if [[ ! -e "$1" ]]; then
        echo "FAIL: missing $1"
        fail=1
    else
        echo "OK: $1"
    fi
}

echo "== Verify artefacts under ${ART} (${OS}) =="

need "${ART}/VST3/AETHR.vst3"

case "${OS}" in
    macOS)
        need "${ART}/AU/AETHR.component"
        need "${ART}/Standalone/AETHR.app"
        ;;
    Windows)
        need "${ART}/Standalone/AETHR.exe"
        ;;
    *)
        echo "FAIL: unknown OS '${OS}'"
        exit 1
        ;;
esac

exit "${fail}"
