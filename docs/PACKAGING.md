# AETHR — Packaging

## Audit snapshot (source of truth)

| Item | Value |
|------|-------|
| CMake project | `project(AETHR VERSION …)` — currently **0.95.0** |
| JUCE plugin target | `Aethr` (`juce_add_plugin`) |
| Formats | **VST3**, **Standalone**, **AU** (Apple only) |
| Product name | `AETHR` (`AETHR_PRODUCT_NAME`) |
| Company | `ixmuk` |
| Bundle ID | `com.ixmuk.aethr` |
| Manufacturer / plugin codes | `Ixmk` / `Aetr` |
| Artefacts | `build/<preset>/Aethr_artefacts/<Config>/{VST3,AU,Standalone}/` |
| Shipping presets | `release` (macOS universal), `release-native` / Windows Release |

Do not invent alternate target names or artefact paths.

## Flow

```
configure → build (Release) → test → pluginval (macOS) → package → validate → dist/
```

| Platform | Primary artefact |
|----------|------------------|
| macOS | `dist/macos/AETHR-<ver>-macOS.dmg` (contains `Install AETHR.pkg`) |
| Windows | `dist/windows/AETHR-<ver>-Windows.exe` (Inno Setup) |

Checksums: `dist/AETHR-<ver>-SHA256SUMS.txt`

## Local commands

### macOS (universal Release)

```bash
cmake --preset release
cmake --build --preset release --parallel
ctest --test-dir build/release --output-on-failure   # if tests enabled
cmake --build --preset release --target package-macos
```

Or:

```bash
./Tools/package_macos.sh   # after exporting AETHR_* env vars — prefer the CMake target
```

### Windows

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --parallel
cmake --build --preset windows-release --target package-windows
```

Requires [Inno Setup 6](https://jrsoftware.org/isinfo.php) (`choco install innosetup`).

## Signing

| Platform | Env / secrets | Result |
|----------|---------------|--------|
| macOS | `APPLE_DEVELOPER_ID`, optional notary API/Apple ID vars | `SIGNED` / `NOTARIZED` |
| Windows | `WINDOWS_CERT_PATH`, `WINDOWS_CERT_PASSWORD` | `SIGNED` |
| (none) | — | `UNSIGNED` (still buildable) |

Never commit certificates or passwords. See `docs/CI.md` and `docs/RELEASE.md`.

## Install locations

Documented in [`INSTALLATION.md`](INSTALLATION.md).
