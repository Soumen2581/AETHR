#!/usr/bin/env bash
# Validate a built macOS DMG + PKG contain the expected AETHR payloads.
# Usage: Tools/validate_macos_package.sh <dmg> <pkg> <version>
set -euo pipefail

DMG="${1:?dmg path}"
PKG="${2:?pkg path}"
VER="${3:?version}"
PRODUCT="${AETHR_PRODUCT_NAME:-AETHR}"

fail() { echo "FAIL: $*" >&2; exit 1; }
[[ -f "${DMG}" ]] || fail "DMG missing: ${DMG}"
[[ -f "${PKG}" ]] || fail "PKG missing: ${PKG}"

# Size sanity
DMG_SIZE="$(stat -f%z "${DMG}" 2>/dev/null || stat -c%s "${DMG}")"
PKG_SIZE="$(stat -f%z "${PKG}" 2>/dev/null || stat -c%s "${PKG}")"
[[ "${DMG_SIZE}" -gt 1000000 ]] || fail "DMG suspiciously small (${DMG_SIZE} bytes)"
[[ "${PKG_SIZE}" -gt 1000000 ]] || fail "PKG suspiciously small (${PKG_SIZE} bytes)"

MOUNT="$(mktemp -d "${TMPDIR:-/tmp}/aethr-dmg.XXXXXX")"
cleanup() {
  hdiutil detach "${MOUNT}" >/dev/null 2>&1 || true
  rmdir "${MOUNT}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

hdiutil attach "${DMG}" -mountpoint "${MOUNT}" -nobrowse -readonly >/dev/null
[[ -f "${MOUNT}/Install ${PRODUCT}.pkg" ]] || fail "DMG missing Install ${PRODUCT}.pkg"
[[ -e "${MOUNT}/Applications" ]] || fail "DMG missing Applications shortcut"
[[ -f "${MOUNT}/README.txt" ]] || fail "DMG missing README.txt"

# Expand pkg and check payloads (no admin install required).
EXPAND="$(mktemp -d "${TMPDIR:-/tmp}/aethr-pkgx.XXXXXX")"
trap 'hdiutil detach "${MOUNT}" >/dev/null 2>&1 || true; rm -rf "${EXPAND}"; rmdir "${MOUNT}" >/dev/null 2>&1 || true' EXIT

pkgutil --expand "${PKG}" "${EXPAND}/expanded"
# Flattened component pkgs put Payload as cpio/gz
PAYLOAD="$(find "${EXPAND}/expanded" -name Payload | head -1)"
[[ -n "${PAYLOAD}" ]] || fail "PKG has no Payload"
PAYLOAD_DIR="${EXPAND}/payload"
mkdir -p "${PAYLOAD_DIR}"
# macOS pkg Payload is typically gzip+cpio
if ! ( cd "${PAYLOAD_DIR}" && cat "${PAYLOAD}" | gzip -d 2>/dev/null | cpio -i 2>/dev/null ); then
  # xar/pbzx variants — try ditto via pkgutil --expand-full when available
  rm -rf "${EXPAND}/expanded" "${PAYLOAD_DIR}"
  mkdir -p "${PAYLOAD_DIR}"
  if pkgutil --expand-full "${PKG}" "${EXPAND}/full" 2>/dev/null; then
    PAYLOAD_DIR="${EXPAND}/full"
  else
    fail "Could not expand PKG payload"
  fi
fi

APP_BIN="$(find "${PAYLOAD_DIR}" -path "*/Applications/${PRODUCT}.app/Contents/MacOS/${PRODUCT}" | head -1)"
VST_BIN="$(find "${PAYLOAD_DIR}" -path "*/VST3/${PRODUCT}.vst3/Contents/MacOS/${PRODUCT}" | head -1)"
AU_BIN="$(find "${PAYLOAD_DIR}" -path "*/Components/${PRODUCT}.component/Contents/MacOS/${PRODUCT}" | head -1)"

[[ -n "${APP_BIN}" && -f "${APP_BIN}" ]] || fail "PKG missing Standalone binary"
[[ -n "${VST_BIN}" && -f "${VST_BIN}" ]] || fail "PKG missing VST3 binary"
[[ -n "${AU_BIN}" && -f "${AU_BIN}" ]] || fail "PKG missing AU binary"

file "${APP_BIN}" | grep -Eq "Mach-O|executable" || fail "Standalone is not a Mach-O binary"

echo "OK: DMG mounts and contains installer + README"
echo "OK: PKG ${VER} contains Standalone + VST3 + AU binaries"
