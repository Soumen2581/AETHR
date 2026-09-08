# AETHR — Install guide (macOS & Windows)

AETHR by **ixmuk** runs on:

| Platform | Formats |
|----------|---------|
| **macOS** 11+ (Apple Silicon or Intel) | VST3, Audio Unit, Standalone |
| **Windows** 10/11 (64-bit) | VST3, Standalone |

Audio Unit is Apple-only. On Windows you use **VST3** (or the Standalone app).

---

## macOS — install with Terminal

### 1) Build (from the repo root)

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

### 2) Install VST3 + AU into your user plug-in folders

```bash
# From the AETHR repo root
ART="build/dev/Aethr_artefacts/RelWithDebInfo"

mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$HOME/Applications"

# Replace any previous install
rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/AETHR.vst3"
rm -rf "$HOME/Library/Audio/Plug-Ins/Components/AETHR.component"

cp -R "$ART/VST3/AETHR.vst3" \
  "$HOME/Library/Audio/Plug-Ins/VST3/"

cp -R "$ART/AU/AETHR.component" \
  "$HOME/Library/Audio/Plug-Ins/Components/"

# Optional: Standalone app
cp -R "$ART/Standalone/AETHR.app" \
  "$HOME/Applications/"

# Clear quarantine / fix ad-hoc signature (needed on some Macs)
./Tools/sanitise-macos-bundle.sh \
  "$HOME/Library/Audio/Plug-Ins/VST3/AETHR.vst3" \
  "$HOME/Library/Audio/Plug-Ins/Components/AETHR.component" \
  "$HOME/Applications/AETHR.app"

# Confirm they landed
ls -la "$HOME/Library/Audio/Plug-Ins/VST3/AETHR.vst3"
ls -la "$HOME/Library/Audio/Plug-Ins/Components/AETHR.component"
```

### 3) Or install from a downloaded CI zip

After downloading `AETHR-macOS-VST3.zip` / `AETHR-macOS-AU.zip` from
[Actions](https://github.com/Soumen2581/AETHR/actions) into `~/Downloads`:

```bash
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"

# Adjust the zip names if yours differ
unzip -o "$HOME/Downloads/AETHR-macOS-VST3.zip" -d /tmp/aethr-mac
unzip -o "$HOME/Downloads/AETHR-macOS-AU.zip" -d /tmp/aethr-mac

rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/AETHR.vst3"
rm -rf "$HOME/Library/Audio/Plug-Ins/Components/AETHR.component"

# Find the bundles inside the unzipped tree and install them
find /tmp/aethr-mac -name 'AETHR.vst3' -type d -maxdepth 3 \
  -exec cp -R {} "$HOME/Library/Audio/Plug-Ins/VST3/" \;
find /tmp/aethr-mac -name 'AETHR.component' -type d -maxdepth 3 \
  -exec cp -R {} "$HOME/Library/Audio/Plug-Ins/Components/" \;

xattr -cr "$HOME/Library/Audio/Plug-Ins/VST3/AETHR.vst3" \
          "$HOME/Library/Audio/Plug-Ins/Components/AETHR.component" 2>/dev/null || true
```

### 4) Rescan in your DAW

Ableton Live example — Preferences → Plug-Ins → **Rescan**.  
Then **remove and re-add** AETHR on the track so the editor reloads.

Logic Pro (AU check):

```bash
auval -v aumu Aetr Ixmk
```

---

## Windows — install with Command Prompt / PowerShell

Use **Command Prompt**, **PowerShell**, or **x64 Native Tools Command Prompt**.

### 1) Build (from the repo root)

In an **x64 Native Tools** shell (or after `vcvars64.bat`):

```bat
cd /d C:\path\to\AETHR
cmake --preset windows
cmake --build --preset windows
ctest --preset windows
```

### 2) Install VST3 (Command Prompt)

```bat
:: From the AETHR repo root
set ART=build\windows\Aethr_artefacts\RelWithDebInfo
set VST3DIR=%CommonProgramFiles%\VST3

if not exist "%VST3DIR%" mkdir "%VST3DIR%"
if exist "%VST3DIR%\AETHR.vst3" rmdir /s /q "%VST3DIR%\AETHR.vst3"

xcopy /E /I /Y "%ART%\VST3\AETHR.vst3" "%VST3DIR%\AETHR.vst3"

:: Optional: Standalone
if not exist "%ProgramFiles%\ixmuk\AETHR" mkdir "%ProgramFiles%\ixmuk\AETHR"
copy /Y "%ART%\Standalone\AETHR.exe" "%ProgramFiles%\ixmuk\AETHR\AETHR.exe"

dir "%VST3DIR%\AETHR.vst3"
```

> `C:\Program Files\...` needs an **Administrator** Command Prompt.  
> For a per-user install (no admin):

```bat
set ART=build\windows\Aethr_artefacts\RelWithDebInfo
set VST3DIR=%LOCALAPPDATA%\Programs\Common\VST3

if not exist "%VST3DIR%" mkdir "%VST3DIR%"
if exist "%VST3DIR%\AETHR.vst3" rmdir /s /q "%VST3DIR%\AETHR.vst3"
xcopy /E /I /Y "%ART%\VST3\AETHR.vst3" "%VST3DIR%\AETHR.vst3"
```

Then add that folder in your DAW’s VST3 search paths if it isn’t listed already.

### 3) Same install in PowerShell

```powershell
# From the AETHR repo root (admin if installing under Program Files)
$art = "build\windows\Aethr_artefacts\RelWithDebInfo"
$vst3 = "$env:CommonProgramFiles\VST3"

New-Item -ItemType Directory -Force -Path $vst3 | Out-Null
Remove-Item -Recurse -Force "$vst3\AETHR.vst3" -ErrorAction SilentlyContinue
Copy-Item -Recurse -Force "$art\VST3\AETHR.vst3" "$vst3\AETHR.vst3"

New-Item -ItemType Directory -Force -Path "$env:ProgramFiles\ixmuk\AETHR" | Out-Null
Copy-Item -Force "$art\Standalone\AETHR.exe" "$env:ProgramFiles\ixmuk\AETHR\AETHR.exe"

Get-ChildItem "$vst3\AETHR.vst3"
```

### 4) Or install from a downloaded CI zip (PowerShell)

```powershell
# After downloading AETHR-Windows-VST3.zip from Actions into Downloads
$zip = "$env:USERPROFILE\Downloads\AETHR-Windows-VST3.zip"
$tmp = "$env:TEMP\aethr-win"
$vst3 = "$env:CommonProgramFiles\VST3"   # use $env:LOCALAPPDATA\Programs\Common\VST3 for user install

Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
Expand-Archive -Path $zip -DestinationPath $tmp -Force

New-Item -ItemType Directory -Force -Path $vst3 | Out-Null
Remove-Item -Recurse -Force "$vst3\AETHR.vst3" -ErrorAction SilentlyContinue

$bundle = Get-ChildItem -Path $tmp -Filter "AETHR.vst3" -Directory -Recurse | Select-Object -First 1
Copy-Item -Recurse -Force $bundle.FullName "$vst3\AETHR.vst3"

Get-ChildItem "$vst3\AETHR.vst3"
```

### 5) Rescan in your DAW

Rescan VST3 plug-ins (Ableton / FL / Cubase / Reaper / Bitwig), then restart the host if AETHR still doesn’t appear.

---

## Where it ends up

| Platform | Format | Path |
|----------|--------|------|
| macOS | VST3 | `~/Library/Audio/Plug-Ins/VST3/AETHR.vst3` |
| macOS | AU | `~/Library/Audio/Plug-Ins/Components/AETHR.component` |
| macOS | Standalone | `~/Applications/AETHR.app` |
| Windows | VST3 | `C:\Program Files\Common Files\VST3\AETHR.vst3` |
| Windows | VST3 (user) | `%LOCALAPPDATA%\Programs\Common\VST3\AETHR.vst3` |
| Windows | Standalone | `C:\Program Files\ixmuk\AETHR\AETHR.exe` |

---

## Get a CI build (optional)

1. Open **[Actions](https://github.com/Soumen2581/AETHR/actions)**.
2. Open the latest green **CI** run.
3. Download **`AETHR-macOS`** or **`AETHR-Windows`**, then use the zip commands above.

---

## First launch

1. Load **AETHR** on a MIDI / instrument track.
2. Send MIDI — the footer should show activity.
3. Start on **String**, then try other engines; optional **ARP** under the engine strip.

Company in the UI: **IXMUK** · Codes: manufacturer `Ixmk`, plugin `Aetr`

---

## Troubleshooting

| Symptom | Terminal fix |
|---------|----------------|
| macOS quarantine / won’t load | `./Tools/sanitise-macos-bundle.sh ~/Library/Audio/Plug-Ins/VST3/AETHR.vst3` |
| Old UI after rebuild (Ableton) | Re-run install commands, then remove + re-add the device |
| Windows “Access denied” | Open Command Prompt **as Administrator**, or use the `%LOCALAPPDATA%` path |
| Plug-in not listed | Confirm the path with `ls` / `dir`, then rescan the DAW |
| Wrong Windows build folder | If you used `-B build\ci`, set `ART=build\ci\Aethr_artefacts\RelWithDebInfo` |

More build options: [`BUILD.md`](BUILD.md).
