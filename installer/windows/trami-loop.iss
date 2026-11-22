#ifndef MyAppVersion
  #define MyAppVersion "0.4.0"
#endif
#ifndef MyAppExePath
  #define MyAppExePath "..\..\build\src\TramiLoop_artefacts\Release\TramiLoop.exe"
#endif
#ifndef MyOutputDir
  #define MyOutputDir "..\..\artifacts"
#endif

[Setup]
AppId={{8F4B9D2A-3C7E-4A1E-9B62-7D5E8C1F4A90}
AppName=TramiLoop
AppVersion={#MyAppVersion}
AppVerName=TramiLoop v{#MyAppVersion}
AppPublisher=trami
AppSupportURL=https://github.com/bischoffdev/trami-loop
DefaultDirName={autopf}\TramiLoop
DefaultGroupName=TramiLoop
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir={#MyOutputDir}
OutputBaseFilename=TramiLoop-v{#MyAppVersion}-Windows-Setup
SetupIconFile=..\..\assets\icon.ico
UninstallDisplayIcon={app}\TramiLoop.exe
LicenseFile=..\..\LICENSE
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#MyAppExePath}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\TramiLoop"; Filename: "{app}\TramiLoop.exe"
Name: "{autodesktop}\TramiLoop"; Filename: "{app}\TramiLoop.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.trami"; ValueType: string; ValueName: ""; ValueData: "TramiLoopProject"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\TramiLoopProject"; ValueType: string; ValueName: ""; ValueData: "TramiLoop Project"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\TramiLoopProject\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\TramiLoop.exe,0"
Root: HKA; Subkey: "Software\Classes\TramiLoopProject\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\TramiLoop.exe"" ""%1"""

[Run]
Filename: "{app}\TramiLoop.exe"; Description: "{cm:LaunchProgram,TramiLoop}"; Flags: nowait postinstall skipifsilent
