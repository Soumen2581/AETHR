#!/usr/bin/env bash
# Local release preflight — mirrors the RELEASE.md checklist where automation helps.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

echo "== AETHR release preflight =="

CMAKE_VER="$(grep -E '^\s*VERSION\s+[0-9]' CMakeLists.txt | head -1 | awk '{print $2}')"
echo "CMake VERSION: ${CMAKE_VER}"

if [[ ! "${CMAKE_VER}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "ERROR: CMake VERSION is not semver X.Y.Z: '${CMAKE_VER}'"
  exit 1
fi

chmod +x Tools/ci-static-check.sh
./Tools/ci-static-check.sh

cmake --preset ci
cmake --build --preset ci --parallel
ctest --preset ci --output-on-failure --parallel

OS_NAME="$(uname -s)"
case "${OS_NAME}" in
  Darwin) VERIFY_OS=macOS ;;
  MINGW*|MSYS*|CYGWIN*|Windows_NT) VERIFY_OS=Windows ;;
  *)
    echo "Skipping artefact verify on unsupported host OS: ${OS_NAME}"
    VERIFY_OS=""
    ;;
esac

if [[ -n "${VERIFY_OS}" ]]; then
  chmod +x Tools/ci-verify-artefacts.sh
  ./Tools/ci-verify-artefacts.sh build/ci/Aethr_artefacts/RelWithDebInfo "${VERIFY_OS}"
fi

echo
echo "Preflight OK for VERSION ${CMAKE_VER}."
echo "Next: complete manual smoke items in docs/RELEASE.md, then:"
echo "  git tag v${CMAKE_VER} && git push origin v${CMAKE_VER}"
