# AETHR — Continuous integration

CI is the automated quality gate for https://github.com/Soumen2581/AETHR.

## Workflows

| Workflow | File | When |
|----------|------|------|
| **CI** | `.github/workflows/ci.yml` | Push / PR / manual |
| **Release** | `.github/workflows/release.yml` | Tag `v*` / manual |

### CI jobs

1. **Static checks** — tracked junk, forbidden branding, basic ID hygiene  
2. **macOS** — configure → build → ctest → verify VST3/AU/Standalone → pluginval → zip artefacts  
3. **Windows** — configure → build → ctest → verify VST3/Standalone → zip artefacts  

Permissions are `contents: read` for CI. Release needs `contents: write` to attach assets.

Concurrency cancels obsolete runs on the same ref so only the latest commit is built.

## Local ≈ CI

```bash
# Hygiene (optional)
./Tools/ci-static-check.sh

# Same flags as Actions
cmake --preset ci
cmake --build --preset ci --parallel
ctest --preset ci --output-on-failure

# Confirm bundles/exes exist
./Tools/ci-verify-artefacts.sh build/ci/Aethr_artefacts/RelWithDebInfo macOS   # or Windows
```

Day-to-day development still uses `cmake --preset dev` (copies into user plug-in folders on macOS).

Windows developers: open an **x64 Native Tools** shell, then use `cmake --preset ci` (or `windows`).

## What the tests cover

`ctest --preset ci` runs the Catch2 suite registered from `Tests/`:

| Area | Tags / files |
|------|----------------|
| Guards / NaN / denormals | `[guards]` |
| Pitch / resonator / exciter | `[pitch]`, resonator, exciter tests |
| Processor / UI construct | `[processor]`, editor create/destroy |
| Engines (all 13) | `[engine][architecture]` |
| Engine switch + extremes | `[engine][regression]` |
| Arp | `[engine][arp]` |
| State round-trip | `[parameters][state]` |
| Fuzz / stability | `[engine][fuzz]`, `[engine][stability]` |

A green CI means configure, build, and this suite succeeded on **both** macOS and Windows. Artefact verification also fails the job if VST3 (and AU/Standalone as required) are missing.

## Plugin validation

On **macOS**, CI downloads pinned **pluginval 1.0.4** via `Tools/fetch-pluginval.sh` and validates the VST3 at strictness **8** (in-process, GUI tests skipped).

**Limitation:** pluginval is not run on Windows CI yet (same binary is available, but the macOS path is the validated one historically). Windows still builds VST3, runs the full unit suite, and verifies the `.vst3` folder + Standalone `.exe` exist.

The 64-voice pool is heap-allocated inside `VoiceEngine` so constructing `AethrProcessor` on the default Windows thread stack (used by Catch2) does not overflow. The test binary also requests an 8 MiB stack on MSVC as a safety margin.

## Artefacts

Successful CI uploads:

- `AETHR-macOS` → `AETHR-macOS-VST3.zip`, `AETHR-macOS-AU.zip`, `AETHR-macOS-Standalone.zip`
- `AETHR-Windows` → `AETHR-Windows-VST3.zip`, `AETHR-Windows-Standalone.zip`

Failed runs upload `ci-logs-<platform>` (CTest / pluginval output) when present.

Install from artefacts using the commands in [`INSTALL.md`](INSTALL.md).

## Releases

Push a tag:

```bash
git tag v0.1.0
git push origin v0.1.0
```

The Release workflow builds both platforms with `cmake --preset ci`, tests, packages:

- `AETHR-v0.1.0-macOS.zip`
- `AETHR-v0.1.0-Windows.zip`

and attaches them to the GitHub Release for that tag.

Version source of truth: `project(AETHR VERSION …)` in `CMakeLists.txt` (injected as `AETHR_VERSION_STRING`). Keep the Git tag aligned with that version when cutting a release.

## Diagnosing failures

1. Open the failed job → read the named step (`Configure`, `Build`, `Unit / DSP / Engine tests`, …).  
2. Download `ci-logs-*` if present.  
3. Reproduce locally with `cmake --preset ci` + `ctest --preset ci`.  
4. Do **not** weaken tests to silence a real DSP/engine failure.

## Caching

FetchContent / `External/JUCE` / `External/Catch2` are cached per OS keyed by CMake files. Build outputs are **not** cached — every run compiles. Delete the Actions cache if a bad dependency tree is suspected.
