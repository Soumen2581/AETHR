# Tools/package_windows.ps1 — build AETHR-x.y.z-Windows.exe via Inno Setup 6
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$ProductName = "AETHR",
    [string]$CompanyName = "ixmuk",
    [Parameter(Mandatory = $true)][string]$ArtefactsDir,
    [Parameter(Mandatory = $true)][string]$DistDir,
    [Parameter(Mandatory = $true)][string]$SourceDir
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    Write-Error "ERROR: $Message"
    exit 1
}

$vst3 = Join-Path $ArtefactsDir "VST3\$ProductName.vst3"
$exe  = Join-Path $ArtefactsDir "Standalone\$ProductName.exe"
if (-not (Test-Path $vst3)) { Fail "missing VST3: $vst3" }
if (-not (Test-Path $exe))  { Fail "missing Standalone: $exe" }

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

$iscc = $null
foreach ($candidate in @(
        "${env:LocalAppData}\Programs\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
    )) {
    if (Test-Path $candidate) { $iscc = $candidate; break }
}
if (-not $iscc) {
    Fail "Inno Setup 6 (ISCC.exe) not found. Install with: choco install innosetup -y"
}

$icon = Join-Path $SourceDir "Assets\icons\aethr.ico"
if (-not (Test-Path $icon)) { Fail "missing icon: $icon" }

$iss = Join-Path $SourceDir "packaging\windows\AETHR.iss"
$args = @(
    "/DMyAppVersion=$Version",
    "/DMyAppName=$ProductName",
    "/DMyCompany=$CompanyName",
    "/DArtefactsDir=$ArtefactsDir",
    "/DDistDir=$DistDir",
    "/DSourceDir=$SourceDir",
    "/DIconFile=$icon",
    $iss
)

Write-Host "== Compiling Inno Setup installer =="
& $iscc @args
if ($LASTEXITCODE -ne 0) { Fail "ISCC failed with exit $LASTEXITCODE" }

$installer = Join-Path $DistDir "$ProductName-$Version-Windows.exe"
if (-not (Test-Path $installer)) { Fail "installer not created: $installer" }

$size = (Get-Item $installer).Length
if ($size -lt 1000000) { Fail "installer suspiciously small ($size bytes)" }

# Optional Authenticode
$signStatus = "UNSIGNED"
if ($env:WINDOWS_CERT_PATH -and $env:WINDOWS_CERT_PASSWORD) {
    $signtool = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($signtool) {
        & signtool.exe sign /f $env:WINDOWS_CERT_PATH /p $env:WINDOWS_CERT_PASSWORD /tr http://timestamp.digicert.com /td sha256 /fd sha256 $installer
        if ($LASTEXITCODE -eq 0) { $signStatus = "SIGNED" }
    }
}

@"
product=$ProductName
version=$Version
exe=$installer
signing=$signStatus
"@ | Set-Content -Path (Join-Path $DistDir "PACKAGING_STATUS.txt")

Write-Host "OK: $installer"
Write-Host "Signing status: $signStatus"
Get-Item $installer | Format-List FullName, Length
