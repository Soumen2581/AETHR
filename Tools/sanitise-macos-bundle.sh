#!/usr/bin/env bash
#
# Makes a freshly built macOS plug-in bundle loadable.
#
# Two separate macOS problems are handled here, both of which were hit during
# Phase 1 and neither of which indicates anything wrong with the built code.
#
# 1. Quarantine inheritance.
#    When the build is launched from a process that itself carries
#    com.apple.quarantine (an editor or terminal that was downloaded rather than
#    installed from the App Store), every file the build produces inherits the
#    attribute. dyld then refuses to load the bundle:
#
#        code signature ... not valid for use in process:
#        library load disallowed by system policy
#
#    even though `codesign --verify` reports the signature as valid.
#
# 2. Finder metadata re-tagging.
#    A bundle built inside a Finder- or iCloud-managed location (Desktop and
#    Documents are both managed) gets com.apple.FinderInfo re-attached moments
#    after it is cleared, and codesign refuses to sign anything carrying it:
#
#        resource fork, Finder information, or similar detritus not allowed
#
#    Because the re-tagging races with us, clearing and signing is retried a few
#    times rather than attempted once.
#
# Failure here never fails the build: an unsanitised bundle may still be perfectly
# loadable (a linker-signed ad-hoc signature often survives untouched), so the
# script reports and moves on. Shipping builds are signed with a Developer ID and
# notarised, which supersedes all of this.
#
# Usage: sanitise-macos-bundle.sh <bundle> [additional bundles...]

set -uo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    exit 0
fi

readonly MAX_ATTEMPTS=4

sanitise_bundle() {
    local bundle="$1"

    for ((attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt)); do
        # .DS_Store inside a bundle is an unsealed resource and fails verification.
        find "${bundle}" -name '.DS_Store' -delete 2>/dev/null

        # iCloud Desktop/Documents create "Name 2" conflict copies inside the
        # bundle. codesign then fails with "code object is not signed at all
        # In subcomponent: .../PkgInfo 2" and hosts hide the plug-in.
        find "${bundle}" \( -name '* 2' -o -name '* 2.*' \) -delete 2>/dev/null

        # Clear everything, then specifically remove the two attributes codesign
        # rejects, as close as possible to the signing call to narrow the race.
        # com.apple.provenance is tolerated by codesign and can be left alone.
        xattr -cr "${bundle}" 2>/dev/null
        xattr -dr com.apple.FinderInfo "${bundle}" 2>/dev/null
        xattr -dr com.apple.ResourceFork "${bundle}" 2>/dev/null

        if codesign --force --sign - --timestamp=none "${bundle}" >/dev/null 2>&1 \
           && codesign --verify --strict "${bundle}" >/dev/null 2>&1; then
            return 0
        fi
    done

    return 1
}

# Returns success if the bundle's Mach-O executable carries a valid signature.
# This is the property that decides whether a host can load the plug-in. Verifying
# the executable directly sidesteps bundle-level resource sealing, which
# com.apple.FinderInfo breaks without affecting loadability at all.
executable_signature_is_valid() {
    local bundle="$1"
    local executable

    executable="$(find "${bundle}/Contents/MacOS" -maxdepth 1 -type f -perm -u+x 2>/dev/null | head -1)"

    [[ -n "${executable}" ]] && codesign --verify --strict "${executable}" >/dev/null 2>&1
}

for bundle in "$@"; do
    [[ -e "${bundle}" ]] || continue

    sanitise_bundle "${bundle}" && continue

    # Re-signing the bundle did not fully succeed. That is only a real problem if
    # the executable's signature is also bad; otherwise it is Finder metadata noise
    # from building inside a Finder- or iCloud-managed folder, which hosts ignore.
    if executable_signature_is_valid "${bundle}"; then
        echo "note: $(basename "${bundle}") has a valid executable signature but the bundle" \
             "does not pass strict verification (Finder metadata). It will load normally;" \
             "build outside Desktop/Documents if you need clean bundle verification." >&2
    else
        echo "warning: $(basename "${bundle}") has no valid signature and hosts are likely to" \
             "refuse to load it. Build outside a Finder-managed folder (Desktop/Documents)." >&2
    fi
done

# Never fail the build over a development-convenience step.
exit 0
