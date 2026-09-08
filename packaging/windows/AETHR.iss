; AETHR Windows installer — Inno Setup 6
; Compiled by Tools/package_windows.ps1
; Defines injected at compile time: MyAppVersion, MyAppName, MyCompany, ArtefactsDir, DistDir, SourceDir, IconFile

#ifndef MyAppVersion
  #error MyAppVersion must be defined
#endif
#ifndef ArtefactsDir
  #error ArtefactsDir must be defined
#endif

#define MyAppName "AETHR"
#define MyAppPublisher "ixmuk"
#define MyAppURL "https://ixmuk.com"
#define MyAppExeName "AETHR.exe"

[Setup]
AppId={{8F3C2A91-6B4E-4D17-9A5C-7E1B0D4F2A83}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
LicenseFile={#SourceDir}\packaging\windows\LICENSE.txt
OutputDir={#DistDir}
OutputBaseFilename={#MyAppName}-{#MyAppVersion}-Windows
SetupIconFile={#IconFile}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
VersionInfoVersion={#MyAppVersion}.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoProductName={#MyAppName}
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Standalone application
Source: "{#ArtefactsDir}\Standalone\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
; VST3 bundle (directory)
Source: "{#ArtefactsDir}\VST3\{#MyAppName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#MyAppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Docs
Source: "{#SourceDir}\packaging\windows\README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\packaging\windows\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
