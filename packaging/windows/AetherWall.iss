; Build only through tools/Build-Release.ps1, after the payload verifier succeeds.
#ifndef PayloadDir
  #error PayloadDir is required
#endif
#ifndef ReleaseVersion
  #error ReleaseVersion is required
#endif
#ifndef ReleaseOutput
  #error ReleaseOutput is required
#endif

[Setup]
AppId={{A3C7652D-8D30-4A93-B5BF-3701EE0F7A51}
AppName=AetherWall Preview
AppVersion={#ReleaseVersion}
AppPublisher=AetherWall contributors
VersionInfoVersion=0.5.0.0
DefaultDirName={localappdata}\Programs\AetherWall
DisableDirPage=yes
UsePreviousAppDir=no
DefaultGroupName=AetherWall
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.26100
AppMutex=Local\AetherWall.Core.Running,Local\AetherWall.Manager.Running
CloseApplications=no
RestartApplications=no
LicenseFile=..\..\LICENSE
InfoBeforeFile=..\..\docs\PREVIEW_NOTES.md
SetupIconFile=..\..\apps\core\aetherwall.ico
UninstallDisplayIcon={app}\AetherWall.Manager.exe
OutputDir={#ReleaseOutput}
OutputBaseFilename=AetherWall-{#ReleaseVersion}-win-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[CustomMessages]
russian.UnsupportedFolder=Предварительная версия поддерживает только папку установки по умолчанию в профиле пользователя.
english.UnsupportedFolder=This preview supports only the default per-user installation folder.

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\AetherWall"; Filename: "{app}\AetherWall.Manager.exe"
Name: "{group}\AetherWall Debug"; Filename: "{app}\AetherWall-Debug.cmd"
Name: "{autodesktop}\AetherWall"; Filename: "{app}\AetherWall.Manager.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\AetherWall.Manager.exe"; Description: "{cm:LaunchProgram,AetherWall}"; Flags: nowait postinstall skipifsilent unchecked

[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  { Keep program files separate from all user data, including /DIR overrides. }
  if CompareText(RemoveBackslash(ExpandFileName(WizardDirValue)),
    RemoveBackslash(ExpandConstant('{localappdata}\Programs\AetherWall'))) <> 0 then
    Result := CustomMessage('UnsupportedFolder');
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Value, Expected: String;
begin
  if CurUninstallStep = usUninstall then begin
    Expected := '"' + ExpandConstant('{app}\AetherWall.Core.exe') + '"';
    if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run',
      'AetherWall', Value) then begin
      if CompareText(Copy(Value, 1, Length(Expected)), Expected) = 0 then begin
        if (Length(Value) = Length(Expected)) or
          (Copy(Value, Length(Expected) + 1, 1) = ' ') then
          if not RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run',
            'AetherWall') then
            Log('Could not remove this installation''s autostart entry.');
      end;
    end;
  end;
  { No user-data deletion. Inno removes only its installed files and shortcuts. }
end;
