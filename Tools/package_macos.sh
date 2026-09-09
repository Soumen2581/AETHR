#!/usr/bin/env bash
# Build a professional macOS installer DMG for AETHR.
#
# Usage (preferred — CMake target package-macos):
#   Tools/package_macos.sh <version> <product> <company> <bundleId> <artefactsDir> <distDir> <sourceDir>
#
# Or via environment variables (manual runs).
#
# Optional signing / notarization (never committed):
#   APPLE_DEVELOPER_ID, APPLE_INSTALLER_ID, APPLE_TEAM_ID,
#   APPLE_ID / APPLE_APP_SPECIFIC_PASSWORD  OR  APPLE_API_KEY / APPLE_API_KEY_ID / APPLE_API_ISSUER
#
# Output:
#   ${distDir}/AETHR-${version}-macOS.dmg
#   ${distDir}/AETHR-${version}-macOS.pkg
#
set -euo pipefail

strip_quotes() { local v="$1"; v="${v#\"}"; v="${v%\"}"; printf '%s' "$v"; }

if [[ $# -ge 7 ]]; then
  VER="$(strip_quotes "$1")"
  PRODUCT="$(strip_quotes "$2")"
  COMPANY="$(strip_quotes "$3")"
  BUNDLE_ID="$(strip_quotes "$4")"
  ART="$(strip_quotes "$5")"
  DIST="$(strip_quotes "$6")"
  ROOT="$(strip_quotes "$7")"
else
  ROOT="$(strip_quotes "${AETHR_SOURCE_DIR:?AETHR_SOURCE_DIR required (or pass 7 CLI args)}")"
  ART="$(strip_quotes "${AETHR_ARTEFACTS_DIR:?AETHR_ARTEFACTS_DIR required}")"
  DIST="$(strip_quotes "${AETHR_DIST_DIR:?AETHR_DIST_DIR required}")"
  VER="$(strip_quotes "${AETHR_VERSION:?AETHR_VERSION required}")"
  PRODUCT="$(strip_quotes "${AETHR_PRODUCT_NAME:-AETHR}")"
  COMPANY="$(strip_quotes "${AETHR_COMPANY_NAME:-ixmuk}")"
  BUNDLE_ID="$(strip_quotes "${AETHR_BUNDLE_ID:-com.ixmuk.aethr}")"
fi

APP="${ART}/Standalone/${PRODUCT}.app"
VST3="${ART}/VST3/${PRODUCT}.vst3"
AU="${ART}/AU/${PRODUCT}.component"

fail() { echo "ERROR: $*" >&2; exit 1; }

echo "package_macos: VER=${VER} PRODUCT=${PRODUCT} ART=${ART}"

[[ -d "${APP}" ]]  || fail "missing Standalone: ${APP}"
[[ -d "${VST3}" ]] || fail "missing VST3: ${VST3}"
[[ -d "${AU}" ]]   || fail "missing AU: ${AU}"

WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/aethr-pkg.XXXXXX")"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

PKGROOT="${WORKDIR}/pkgroot"
DMGSTAGE="${WORKDIR}/dmg"
SCRIPTS="${WORKDIR}/scripts"
mkdir -p \
  "${PKGROOT}/Applications" \
  "${PKGROOT}/Library/Audio/Plug-Ins/VST3" \
  "${PKGROOT}/Library/Audio/Plug-Ins/Components" \
  "${DMGSTAGE}" \
  "${SCRIPTS}" \
  "${DIST}"

echo "== Staging install root =="
ditto "${APP}"  "${PKGROOT}/Applications/${PRODUCT}.app"
ditto "${VST3}" "${PKGROOT}/Library/Audio/Plug-Ins/VST3/${PRODUCT}.vst3"
ditto "${AU}"   "${PKGROOT}/Library/Audio/Plug-Ins/Components/${PRODUCT}.component"

# Strip quarantine / ad-hoc re-sign for local usability (Developer ID overrides later).
if [[ -x "${ROOT}/Tools/sanitise-macos-bundle.sh" ]]; then
  bash "${ROOT}/Tools/sanitise-macos-bundle.sh" "${PKGROOT}/Applications/${PRODUCT}.app" || true
  bash "${ROOT}/Tools/sanitise-macos-bundle.sh" "${PKGROOT}/Library/Audio/Plug-Ins/VST3/${PRODUCT}.vst3" || true
  bash "${ROOT}/Tools/sanitise-macos-bundle.sh" "${PKGROOT}/Library/Audio/Plug-Ins/Components/${PRODUCT}.component" || true
fi

# Apply brand icon to the app bundle when present.
ICNS="${ROOT}/Assets/icons/AETHR.icns"
if [[ -f "${ICNS}" ]]; then
  mkdir -p "${PKGROOT}/Applications/${PRODUCT}.app/Contents/Resources"
  cp "${ICNS}" "${PKGROOT}/Applications/${PRODUCT}.app/Contents/Resources/Icon.icns"
  /usr/libexec/PlistBuddy -c "Set :CFBundleIconFile Icon" \
    "${PKGROOT}/Applications/${PRODUCT}.app/Contents/Info.plist" 2>/dev/null \
    || /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string Icon" \
         "${PKGROOT}/Applications/${PRODUCT}.app/Contents/Info.plist" 2>/dev/null \
    || true
fi

SIGN_STATUS="UNSIGNED"
if [[ -n "${APPLE_DEVELOPER_ID:-}" ]]; then
  echo "== Codesign (Developer ID) =="
  SIGN_IDENTITY="${APPLE_DEVELOPER_ID}"
  codesign --force --deep --options runtime --timestamp \
    --sign "${SIGN_IDENTITY}" \
    "${PKGROOT}/Library/Audio/Plug-Ins/VST3/${PRODUCT}.vst3"
  codesign --force --deep --options runtime --timestamp \
    --sign "${SIGN_IDENTITY}" \
    "${PKGROOT}/Library/Audio/Plug-Ins/Components/${PRODUCT}.component"
  codesign --force --deep --options runtime --timestamp \
    --sign "${SIGN_IDENTITY}" \
    "${PKGROOT}/Applications/${PRODUCT}.app"
  SIGN_STATUS="SIGNED"
else
  echo "== Codesign skipped (APPLE_DEVELOPER_ID unset) — package will be UNSIGNED =="
fi

COMPONENT_PKG="${WORKDIR}/${PRODUCT}-component.pkg"
FINAL_PKG="${DIST}/${PRODUCT}-${VER}-macOS.pkg"

cat > "${SCRIPTS}/postinstall" <<'EOS'
#!/bin/bash
# Refresh Launch Services / AU cache hints after install.
set -euo pipefail
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister \
  -f "/Applications/AETHR.app" >/dev/null 2>&1 || true
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
exit 0
EOS
chmod 755 "${SCRIPTS}/postinstall"

echo "== pkgbuild =="
pkgbuild \
  --root "${PKGROOT}" \
  --identifier "${BUNDLE_ID}.pkg" \
  --version "${VER}" \
  --install-location "/" \
  --scripts "${SCRIPTS}" \
  "${COMPONENT_PKG}"

# Optional productsign if installer identity provided separately.
if [[ -n "${APPLE_INSTALLER_ID:-}" ]]; then
  productsign --sign "${APPLE_INSTALLER_ID}" "${COMPONENT_PKG}" "${FINAL_PKG}"
  SIGN_STATUS="SIGNED"
else
  cp "${COMPONENT_PKG}" "${FINAL_PKG}"
fi

# Notarize when credentials exist.
if [[ "${SIGN_STATUS}" == "SIGNED" ]] && { [[ -n "${APPLE_API_KEY:-}" ]] || [[ -n "${APPLE_ID:-}" ]]; }; then
  echo "== Notarytool submit =="
  ZIP_FOR_NOTARY="${WORKDIR}/${PRODUCT}-${VER}-pkg.zip"
  ditto -c -k --keepParent "${FINAL_PKG}" "${ZIP_FOR_NOTARY}"
  if [[ -n "${APPLE_API_KEY:-}" && -n "${APPLE_API_KEY_ID:-}" && -n "${APPLE_API_ISSUER:-}" ]]; then
    xcrun notarytool submit "${ZIP_FOR_NOTARY}" \
      --key "${APPLE_API_KEY}" \
      --key-id "${APPLE_API_KEY_ID}" \
      --issuer "${APPLE_API_ISSUER}" \
      --wait
  else
    xcrun notarytool submit "${ZIP_FOR_NOTARY}" \
      --apple-id "${APPLE_ID}" \
      --team-id "${APPLE_TEAM_ID:?APPLE_TEAM_ID required with APPLE_ID}" \
      --password "${APPLE_APP_SPECIFIC_PASSWORD:?APPLE_APP_SPECIFIC_PASSWORD required}" \
      --wait
  fi
  xcrun stapler staple "${FINAL_PKG}"
  SIGN_STATUS="NOTARIZED"
  echo "== Notarization stapled =="
else
  echo "== Notarization skipped (no signing credentials) — status ${SIGN_STATUS} =="
fi

# DMG stage: installer pkg + README + Applications shortcut for polish.
cp "${FINAL_PKG}" "${DMGSTAGE}/Install ${PRODUCT}.pkg"
ln -s /Applications "${DMGSTAGE}/Applications"

README="${DMGSTAGE}/README.txt"
cat > "${README}" <<EOF
${PRODUCT} ${VER}
${COMPANY} — Physical Resonance Engine

INSTALL
=======
1. Double-click "Install ${PRODUCT}.pkg"
2. Authenticate when macOS asks for permission
3. The installer places:
   • ${PRODUCT}.app          → /Applications/
   • ${PRODUCT}.vst3         → /Library/Audio/Plug-Ins/VST3/
   • ${PRODUCT}.component    → /Library/Audio/Plug-Ins/Components/

4. Rescan plugins in your DAW (or relaunch the DAW)

USER PRESETS
============
User-saved .aethr presets live under your home Library and are never removed
by the uninstaller / a later upgrade.

SIGNING STATUS
==============
This build is: ${SIGN_STATUS}

If Gatekeeper blocks an UNSIGNED build:
  Right-click the pkg → Open, or clear quarantine after download.
  See docs/INSTALLATION.md in the repository.

EOF

VOLNAME="${PRODUCT} ${VER}"
DMG_TMP="${WORKDIR}/${PRODUCT}-${VER}-macOS.dmg"
DMG_OUT="${DIST}/${PRODUCT}-${VER}-macOS.dmg"
rm -f "${DMG_TMP}" "${DMG_OUT}"

echo "== Creating DMG =="
hdiutil create \
  -volname "${VOLNAME}" \
  -srcfolder "${DMGSTAGE}" \
  -ov -format UDZO \
  "${DMG_TMP}"

cp "${DMG_TMP}" "${DMG_OUT}"

echo "== Validating package =="
bash "${ROOT}/Tools/validate_macos_package.sh" "${DMG_OUT}" "${FINAL_PKG}" "${VER}"

# Write status sidecar for CI
cat > "${DIST}/PACKAGING_STATUS.txt" <<EOF
product=${PRODUCT}
version=${VER}
dmg=${DMG_OUT}
pkg=${FINAL_PKG}
signing=${SIGN_STATUS}
EOF

echo "OK: ${DMG_OUT}"
echo "OK: ${FINAL_PKG}"
echo "Signing status: ${SIGN_STATUS}"
ls -lh "${DMG_OUT}" "${FINAL_PKG}"
