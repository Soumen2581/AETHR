#!/usr/bin/env bash
#
# Downloads a pinned pluginval release into Tools/.
#
# pluginval is Tracktion's plugin validator and is the standard way to catch
# lifecycle, threading and state-restoration bugs that a DAW would only surface
# intermittently. The version is pinned so validation results are comparable
# between machines and over time.
#
# Usage: Tools/fetch-pluginval.sh [version]

set -euo pipefail

PLUGINVAL_VERSION="${1:-v1.0.4}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

case "$(uname -s)" in
    Darwin) ASSET="pluginval_macOS.zip" ;;
    Linux)  ASSET="pluginval_Linux.zip" ;;
    MINGW*|MSYS*|CYGWIN*) ASSET="pluginval_Windows.zip" ;;
    *) echo "Unsupported platform: $(uname -s)" >&2; exit 1 ;;
esac

URL="https://github.com/Tracktion/pluginval/releases/download/${PLUGINVAL_VERSION}/${ASSET}"

echo "Fetching pluginval ${PLUGINVAL_VERSION} from ${URL}"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

curl --fail --location --silent --show-error "${URL}" --output "${TMP_DIR}/${ASSET}"
unzip -q -o "${TMP_DIR}/${ASSET}" -d "${SCRIPT_DIR}"

echo "pluginval ${PLUGINVAL_VERSION} installed under ${SCRIPT_DIR}"
