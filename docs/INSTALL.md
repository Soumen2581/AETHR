# AETHR — Install guide (macOS & Windows)

AETHR by **ixmuk** runs on:

| Platform | Formats |
|----------|---------|
| **macOS** 11+ (Apple Silicon or Intel) | VST3, Audio Unit, Standalone |
| **Windows** 10/11 (64-bit) | VST3, Standalone |

Audio Unit is Apple-only. On Windows you use **VST3** (or the Standalone app).

---

## Option A — Install a CI build (easiest)

Every push to `master` builds both platforms:

1. Open **[Actions](https://github.com/Soumen2581/AETHR/actions)**.
2. Open the latest green **CI** run.
3. Under **Artifacts**, download:
   - `AETHR-macOS` → VST3, AU, Standalone zips  
   - `AETHR-Windows` → VST3 and Standalone zips  
4. Unzip, then follow the platform steps below.

Artefacts expire after GitHub’s retention period; for a lasting copy, keep the zip yourself or cut a Release later.

---

## Option B — Build from source

### Shared requirements

- CMake **≥ 3.25**
- A **C++23** compiler (Apple Clang / MSVC 19.4x)
- **Ninja** recommended
- Internet on first configure (JUCE + Catch2 are fetched automatically)

### macOS

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Built plugins land in:

```text
build/dev/Aethr_artefacts/RelWithDebInfo/VST3/AETHR.vst3
build/dev/Aethr_artefacts/RelWithDebInfo/AU/AETHR.component
build/dev/Aethr_artefacts/RelWithDebInfo/Standalone/AETHR.app
```

With `AETHR_COPY_AFTER_BUILD=ON` (default on macOS), they are also copied into your user plug-in folders (see paths below). After a UI rebuild, **remove and re-add** the device in the DAW — many hosts cache the editor binary.

### Windows

Use an **x64 Native Tools Command Prompt** (or run `vcvars64.bat` first):

```bat
cmake --preset windows
cmake --build --preset windows
ctest --preset windows
```

Or without presets:

```bat
cmake -S . -B build\ci -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DAETHR_WARNINGS_AS_ERRORS=ON -DAETHR_COPY_AFTER_BUILD=OFF -DAETHR_BUILD_AU=OFF
cmake --build build\ci --parallel
ctest --test-dir build\ci --output-on-failure
```

Built plugins land in:

```text
build\windows\Aethr_artefacts\RelWithDebInfo\VST3\AETHR.vst3
build\windows\Aethr_artefacts\RelWithDebInfo\Standalone\AETHR.exe
```

(Use `build\ci\...` if you used the `-B build\ci` commands above.)

---

## Install locations

### macOS

Copy (or confirm) the bundles here:

| Format | User folder (recommended) | System folder |
|--------|---------------------------|---------------|
| **VST3** | `~/Library/Audio/Plug-Ins/VST3/AETHR.vst3` | `/Library/Audio/Plug-Ins/VST3/` |
| **AU** | `~/Library/Audio/Plug-Ins/Components/AETHR.component` | `/Library/Audio/Plug-Ins/Components/` |
| **Standalone** | Anywhere (Applications is fine) | — |

Tips:

- In Finder, **Go → Go to Folder…** and paste `~/Library/Audio/Plug-Ins/`.
- After installing, rescan plug-ins in your DAW (Ableton: Preferences → Plug-Ins → Rescan).
- If the plug-in is missing after a rebuild on an iCloud Desktop, run:

  ```bash
  ./Tools/sanitise-macos-bundle.sh ~/Library/Audio/Plug-Ins/VST3/AETHR.vst3
  ./Tools/sanitise-macos-bundle.sh ~/Library/Audio/Plug-Ins/Components/AETHR.component
  ```

### Windows

| Format | Typical folder |
|--------|----------------|
| **VST3** | `C:\Program Files\Common Files\VST3\AETHR.vst3` |
| **VST3** (user) | `%LOCALAPPDATA%\Programs\Common\VST3\AETHR.vst3` |
| **Standalone** | Any folder you like (e.g. `C:\Program Files\ixmuk\AETHR\`) |

Steps:

1. Unzip `AETHR-Windows-VST3.zip` so you have a folder named `AETHR.vst3` (not nested zips of loose files).
2. Copy that **entire folder** into your VST3 directory.
3. In your DAW, add/rescan that VST3 path if it isn’t already listed.
4. Restart the DAW if the plug-in doesn’t appear.

---

## First launch checklist

1. Create a MIDI track / instrument track and load **AETHR**.
2. Send MIDI — footer should show note activity / voices.
3. Start on **String** engine, then try **Bell**, **Plate**, **Granular**, **Hybrid**.
4. Optional: enable **ARP** under the engine strip, hold a chord, toggle the 16 step gates.

Company name in the UI footer: **IXMUK**.

---

## DAW notes

| Host | Tip |
|------|-----|
| **Ableton Live** | Rescan plug-ins; after updating AETHR, delete the device from the track and re-insert it. |
| **Logic Pro** | AU only on Mac; run `auval -v aumu Aetr Ixmk` if validation fails. |
| **FL Studio / Reaper / Cubase / Bitwig** | Use VST3; point the scanner at the VST3 folder above. |

Manufacturer code: `Ixmk` · Plugin code: `Aetr` · Bundle ID: `com.ixmuk.aethr`

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| Plug-in not listed | Wrong folder; rescan; confirm 64-bit host. |
| Loads then blank / old UI (macOS) | Remove + re-add device; sanitise bundle; clear Ableton’s plug-in cache if needed. |
| Windows Defender blocks `.exe` | Allow the Standalone / VST3 from a trusted build (your CI or local compile). |
| Build fails fetching JUCE | Check network / proxy; delete `External/JUCE` and reconfigure. |
| “Codesign / quarantine” on Mac | Run `Tools/sanitise-macos-bundle.sh` on the installed bundle. |

More build detail: [`BUILD.md`](BUILD.md).
