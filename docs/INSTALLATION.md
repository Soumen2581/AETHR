# AETHR — Installation

**Current release:** [v0.95.0](https://github.com/Soumen2581/AETHR/releases/tag/v0.95.0)

| Platform | Direct download |
|----------|-----------------|
| macOS | [AETHR-0.95.0-macOS.dmg](https://github.com/Soumen2581/AETHR/releases/download/v0.95.0/AETHR-0.95.0-macOS.dmg) |
| Windows | [AETHR-0.95.0-Windows.exe](https://github.com/Soumen2581/AETHR/releases/download/v0.95.0/AETHR-0.95.0-Windows.exe) |
| Checksums | [AETHR-0.95.0-SHA256SUMS.txt](https://github.com/Soumen2581/AETHR/releases/download/v0.95.0/AETHR-0.95.0-SHA256SUMS.txt) |

Version always matches `project(AETHR VERSION …)` in `CMakeLists.txt`.

---

## macOS

### Install

1. Download [AETHR-0.95.0-macOS.dmg](https://github.com/Soumen2581/AETHR/releases/download/v0.95.0/AETHR-0.95.0-macOS.dmg)
2. Open the DMG
3. Double-click **Install AETHR.pkg**
4. Authenticate when prompted
5. Rescan plugins in your DAW (or relaunch the DAW)

### What gets installed

| Component | Location |
|-----------|----------|
| Standalone | `/Applications/AETHR.app` |
| VST3 | `/Library/Audio/Plug-Ins/VST3/AETHR.vst3` |
| Audio Unit | `/Library/Audio/Plug-Ins/Components/AETHR.component` |

### Gatekeeper / unsigned builds

If the release is **UNSIGNED** (no Apple Developer ID secrets in CI):

- Right-click the `.pkg` → **Open**, or
- After download: `xattr -dr com.apple.quarantine ~/Downloads/AETHR-*.dmg`

Signed + notarized builds open normally. Packaging status is written to
`dist/macos/PACKAGING_STATUS.txt` during the build (`UNSIGNED` / `SIGNED` / `NOTARIZED`).

### Uninstall

Remove the three paths above (Admin password may be required for `/Library`).

User presets are **not** under those paths — see [User data](#user-data).

---

## Windows

### Install

1. Download [AETHR-0.95.0-Windows.exe](https://github.com/Soumen2581/AETHR/releases/download/v0.95.0/AETHR-0.95.0-Windows.exe)
2. Run the installer (admin elevation required for system VST3)
3. Optionally create a Desktop shortcut
4. Rescan VST3 plugins in your DAW

### What gets installed

| Component | Location |
|-----------|----------|
| Standalone | `%ProgramFiles%\AETHR\AETHR.exe` |
| VST3 | `%CommonProgramFiles%\VST3\AETHR.vst3\` |

### Uninstall

**Settings → Apps → AETHR → Uninstall**, or Start Menu → AETHR → Uninstall.

Removes application files, VST3, and shortcuts. Does **not** delete user presets.

### SmartScreen / unsigned builds

If Authenticode secrets are not configured, Windows may warn on first run.
Choose **More info → Run anyway** for trusted builds from the official GitHub Release.

---

## User data

Factory presets ship inside the plugin. User-saved `.aethr` files live under the
OS application-data directory for **ixmuk / AETHR** (e.g. macOS
`~/Library/Application Support/ixmuk/AETHR/UserPresets`).

Upgrades and uninstallers must not wipe that folder.

---

## Verify downloads

Releases publish `AETHR-<version>-SHA256SUMS.txt`. Example:

```bash
shasum -a 256 -c AETHR-0.95.0-SHA256SUMS.txt
```

---

## Local packaging (developers)

See [`PACKAGING.md`](PACKAGING.md) and [`RELEASE.md`](RELEASE.md).
