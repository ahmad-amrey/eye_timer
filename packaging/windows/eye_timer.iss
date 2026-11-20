; Eye Timer installer (Inno Setup 6).
;
; Installs for the current user only, into %LOCALAPPDATA%\Programs\Eye Timer,
; so no administrator rights are needed. Running a newer installer updates
; the app in place. Settings live in %LOCALAPPDATA%\EyeTimer and are never
; touched by installing, updating or uninstalling.
;
; Build (after packaging/windows/deploy-msys2.sh has filled dist\EyeTimer):
;   iscc /DAppVersion=0.10.0 /DSourceDir=dist\EyeTimer /DOutputDir=dist packaging\windows\eye_timer.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\EyeTimer"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif

#define AppName "Eye Timer"
#define AppExe "EyeTimer.exe"
#define AppUrl "https://github.com/ahmad-amrey/eye_timer"

[Setup]
; Never change AppId: it is how a newer installer finds and updates this one.
AppId={{6F0B2C8E-4E1D-4C55-9C9B-2E7A6B1D9E31}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Ahmad AlAmrey
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
AppUpdatesURL={#AppUrl}/releases
VersionInfoVersion={#AppVersion}
; Per-user install: {autopf} resolves to %LOCALAPPDATA%\Programs.
PrivilegesRequired=lowest
DefaultDirName={autopf}\{#AppName}
DisableProgramGroupPage=yes
; The folder is always our own, so clearing it on update below is safe.
DisableDirPage=yes
UsePreviousAppDir=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=EyeTimer-windows-x64-setup
SetupIconFile=..\..\assets\icon.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[InstallDelete]
; Clear out the previous version's program files (never the settings folder,
; which lives elsewhere) so removed Qt plugins don't linger after an update.
Type: filesandordirs; Name: "{app}\*"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; The "Start with Windows" shortcut the app creates; it would point at a
; missing file once the app is gone. Settings in %LOCALAPPDATA%\EyeTimer stay.
Type: files; Name: "{userstartup}\{#AppName}.lnk"

[Code]
// The app lives in the tray with no visible window, so Windows' Restart
// Manager can't always ask it to close. End it before files are replaced.
procedure StopRunningApp();
var
  ResultCode: Integer;
begin
  Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM {#AppExe}', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  StopRunningApp();
  Result := '';
end;

function InitializeUninstall(): Boolean;
begin
  StopRunningApp();
  Result := True;
end;
