# AETHR — Build

## Requirements

| Tool | Minimum | Verified working |
| --- | --- | --- |
| CMake | 3.25 | 4.3.2 |
| C++ compiler | C++23 | Apple clang 21.0.0 |
| Generator | any | Ninja 1.13.2 |
| Git | any | 2.52.0 |

Dependencies are fetched by CMake at configure time and pinned to exact tags:
**JUCE 9.0.1**, **Catch2 v3.9.1**. No manual installation is required. If `External/JUCE` already
contains a checkout, it is reused rather than re-downloaded.

The CMake floor is 3.25 rather than JUCE's own 3.22 because the build uses `FetchContent`'s `SYSTEM`
option (3.25) to suppress warnings from dependency headers, and `CMakePresets` schema v6. CMake 4.x
removed support for `cmake_minimum_required` values below 3.5; nothing in this project or its two
dependencies declares such a floor, so CMake 4 works without compatibility shims.

## Continuous integration

See [`CI.md`](CI.md) for the full quality gate (macOS + Windows matrix, pluginval,
artefacts, releases).

Local equivalent of Actions:

```bash
cmake --preset ci
cmake --build --preset ci --parallel
ctest --preset ci --output-on-failure
```

**End-user install steps (folders, DAW rescan, troubleshooting):** see
[`INSTALL.md`](INSTALL.md).

## Quick start

```bash
cmake --preset dev          # configure
cmake --build --preset dev  # build VST3 + AU + Standalone + tests
ctest --preset dev          # run the test suite
```

Artefacts land in `build/dev/Aethr_artefacts/RelWithDebInfo/` and, because
`AETHR_COPY_AFTER_BUILD` defaults to ON, are also installed to the user plug-in folders
(`~/Library/Audio/Plug-Ins/VST3` and `.../Components` on macOS).

## Presets

| Preset | Config | Purpose |
| --- | --- | --- |
| `dev` | RelWithDebInfo, `-Werror` | Daily development. Fast enough to audition, keeps debug info, fails on new warnings. |
| `debug` | Debug, `-Werror` | `jassert` live, no optimisation. Use when tracking down an invalid coefficient or a stability bug. |
| `release` | Release + LTO, macOS universal (arm64 + x86_64) | Shipping builds. |
| `release-native` | Release + LTO, host architecture only | Profiling — a universal binary only doubles build time. |

## Options

| Option | Default | Effect |
| --- | --- | --- |
| `AETHR_BUILD_TESTS` | ON | Build the Catch2 suite and register it with CTest |
| `AETHR_BUILD_STANDALONE` | ON | Build the standalone application |
| `AETHR_BUILD_AU` | ON | Build the Audio Unit (macOS only) |
| `AETHR_WARNINGS_AS_ERRORS` | OFF | Add `-Werror`/`/WX` to first-party files |
| `AETHR_COPY_AFTER_BUILD` | ON | Install to the user plug-in folders after building |
| `AETHR_JUCE_TAG` | `9.0.1` | JUCE tag to build against |
| `AETHR_CATCH2_TAG` | `v3.9.1` | Catch2 tag to build against |

## Renaming the product

All branding is CMake cache variables at the top of `CMakeLists.txt`:
`AETHR_PRODUCT_NAME`, `AETHR_PRODUCT_TAGLINE`, `AETHR_COMPANY_NAME`, `AETHR_BUNDLE_ID`,
`AETHR_MANUFACTURER_ID`, `AETHR_PLUGIN_ID`. They are injected into the source as preprocessor
definitions and read through `Source/Core/Branding.h`, so no source file needs editing.

Note that `AETHR_PLUGIN_ID` and `AETHR_MANUFACTURER_ID` are the 4-character codes hosts use to
identify the plugin. Changing them after release makes existing sessions fail to find the plugin.

## macOS specifics

### Deployment target

`CMAKE_OSX_DEPLOYMENT_TARGET` is **11.0** (Big Sur). Reasoning: macOS 11 is the first release that
supports Apple Silicon, so it is the lowest target for which a universal binary makes sense, and it
still covers Intel machines that stopped receiving updates after Catalina. Going higher would
exclude working studio machines for no engineering benefit — plenty of professional installations
run 12 or 13. Going lower would mean an Intel-only build. The project uses no API newer than 11.0.

### Audio Unit with Command Line Tools only — measured result

This machine has **no full Xcode**, only Command Line Tools (`xcodebuild` is unavailable). The
question of whether AU can still be built was answered empirically rather than assumed:

- **AU v2 builds and validates successfully with Command Line Tools alone.** JUCE's AU wrapper needs
  only the `AudioUnit` and `CoreAudioKit` frameworks from the macOS SDK, both of which the CLT SDK
  provides. `juceaide` also builds and runs correctly.
- **`auval` is available at `/usr/bin/auval`** — it ships with macOS, not with Xcode. Running
  `auval -v aumu Aetr Ixmk` against the built component reports **AU VALIDATION SUCCEEDED**,
  including render tests at 11 025 / 22 050 / 44 100 / 48 000 / 96 000 / 192 000 Hz, block sizes from
  64 to 4096, the deliberately-too-large-block failure case, parameter scheduling and MIDI.
- **AUv3 is not buildable here.** It requires an app-extension target that only Xcode can produce.
  AUv3 is not currently in `AETHR_FORMATS` and is out of scope.

So AU stays enabled by default. `AETHR_BUILD_AU=OFF` exists for environments where it is not wanted.

### Quarantine on build output — and why the build clears it

When the build is launched from a process that itself carries `com.apple.quarantine` (an editor or
terminal that was downloaded rather than installed by the App Store), **every file the build produces
inherits that attribute**. dyld then refuses to load the plugin:

```
code signature ... not valid for use in process:
library load disallowed by system policy
```

This was hit during Phase 1: the VST3 built cleanly, `codesign --verify --deep --strict` reported
*"valid on disk"* and *"satisfies its Designated Requirement"*, and yet pluginval reported
`Unable to load VST-3 plug-in file`. Copying the identical bundle to a path with cleared attributes
made it load immediately, which isolated the cause to the attribute rather than the binary.

The build therefore runs `Tools/sanitise-macos-bundle.sh` as a post-build step for every format,
clearing extended attributes and re-applying an ad-hoc signature to both the build artefact and the
installed copy. If a host ever reports that the plugin cannot be loaded, run it manually:

```bash
Tools/sanitise-macos-bundle.sh build/dev/Aethr_artefacts/RelWithDebInfo/VST3/AETHR.vst3
```

Shipping builds are signed with a Developer ID and notarised instead; ad-hoc signing is a development
convenience only.

### Building inside Desktop or Documents

A related and more stubborn issue: **Desktop and Documents are Finder- and iCloud-managed
locations**, and a bundle built there gets `com.apple.FinderInfo` re-attached moments after it is
cleared. `codesign` refuses to sign anything carrying it:

```
resource fork, Finder information, or similar detritus not allowed
```

Because the re-tagging races with the signing step, `sanitise-macos-bundle.sh` retries a few times.
The race was confirmed directly: the identical bundle copied to `/tmp` signs and verifies cleanly on
the first attempt, while in place on the Desktop it intermittently does not.

Crucially, `com.apple.FinderInfo` breaks *bundle-level* verification but **not loadability** — an AU
carrying it still passes `auval`. The script therefore distinguishes the two cases and only warns
when the executable's own signature is invalid, which is the condition that actually stops a host
loading the plugin.

If you need consistently clean bundle verification — and you will, before notarising — **build
outside Desktop and Documents**:

```bash
cmake -S . -B ~/aethr-build/dev -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build ~/aethr-build/dev
```

## Validation

```bash
Tools/fetch-pluginval.sh                       # once, downloads pinned pluginval v1.0.4
Tools/pluginval.app/Contents/MacOS/pluginval \
    --strictness-level 10 --timeout-ms 180000 \
    --validate build/dev/Aethr_artefacts/RelWithDebInfo/VST3/AETHR.vst3

auval -v aumu Aetr Ixmk                        # macOS only, AU
```

Phase 1 status: **pluginval passes at strictness level 10** (its maximum) and **auval passes**.
See `docs/TESTING.md` for what those cover and what they do not.

## Windows

Supported formats on Windows: **VST3** and **Standalone** (Audio Unit is macOS-only).

```bat
cmake -S . -B build\ci -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DAETHR_WARNINGS_AS_ERRORS=ON -DAETHR_COPY_AFTER_BUILD=OFF -DAETHR_BUILD_AU=OFF
cmake --build build\ci --parallel
ctest --test-dir build\ci --output-on-failure
```

Use an “x64 Native Tools” / `vcvars64.bat` shell so MSVC and Ninja share the same environment.
GitHub Actions builds and tests Windows on every push; download the VST3 zip from the Actions
artefacts tab.
