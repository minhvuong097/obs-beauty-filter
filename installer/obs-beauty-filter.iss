; Inno Setup 6 installer for the OBS Beauty Filter Windows plugin.
;
; Build from the repository root:
;   ISCC.exe /DPluginVersion=1.5.0 installer\obs-beauty-filter.iss
;
; The script installs the OBS Studio 33+ plugin layout:
;   %ProgramData%\obs-studio\plugins\obs-beauty-filter\obs-beauty-filter.dll
;   %ProgramData%\obs-studio\plugins\obs-beauty-filter\data\...
; For a detected portable OBS tree it uses <OBS>\plugins\obs-beauty-filter
; instead, matching OBS' portable plugin resolver.

#ifndef PluginVersion
  #define PluginVersion "1.5.0"
#endif
#ifndef PluginPackageDir
  #define PluginPackageDir "..\package\obs-beauty-filter"
#endif
#ifndef OutputDir
  #define OutputDir ".."
#endif

#define PluginName "obs-beauty-filter"
#define PluginDisplayName "OBS Beauty Filter"
#define PluginPublisher "minhvuong097"
#define PluginUrl "https://github.com/minhvuong097/obs-beauty-filter"

[Setup]
; Keep this AppId stable: it is what makes upgrades replace the previous
; installation and find the existing uninstaller.
AppId={{8C2E1F6A-5B0D-4E37-9F2C-7A4B1D3E6C58}
AppName={#PluginDisplayName}
AppVersion={#PluginVersion}
AppVerName={#PluginDisplayName} {#PluginVersion}
AppPublisher={#PluginPublisher}
AppPublisherURL={#PluginUrl}
AppSupportURL={#PluginUrl}/issues
AppUpdatesURL={#PluginUrl}/releases
VersionInfoVersion={#PluginVersion}.0
DefaultDirName={code:GetDefaultPluginDir}
DisableProgramGroupPage=yes
DefaultGroupName={#PluginDisplayName}
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir={#OutputDir}
OutputBaseFilename=obs-beauty-filter-{#PluginVersion}-windows-x64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\LICENSE
CloseApplications=yes
RestartApplications=no
UninstallDisplayName={#PluginDisplayName}
UninstallDisplayIcon={app}\{#PluginName}.dll
SetupLogging=yes
UsePreviousAppDir=yes

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
en.ObsTooOld=This installer requires OBS Studio 33 or newer, but OBS %1 was detected. Please download the ZIP package and follow the legacy installation steps in the README for OBS 32 and older.
en.ObsNotFound=OBS Studio was not detected in the Windows registry. Setup will install to %1. If you use portable OBS, choose <OBS-portable-root>\plugins\obs-beauty-filter on the next page, or run setup with /OBSDIR=<OBS-portable-root>.
en.LaunchObs=Launch OBS Studio

[Files]
Source: "{#PluginPackageDir}\{#PluginName}.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PluginPackageDir}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: dirifempty; Name: "{app}\data\locale"
Type: dirifempty; Name: "{app}\data"
Type: dirifempty; Name: "{app}"

[Run]
Filename: "{code:GetObsExePath}"; Description: "{cm:LaunchObs}"; Flags: postinstall nowait skipifsilent; Check: CanLaunchObs

[Code]
var
  ObsDir: string;
  ObsMajor: Integer;
  ObsVersionKnown: Boolean;

function AcceptObsDir(const Candidate: string): Boolean;
begin
  Result := (Candidate <> '') and
            FileExists(AddBackslash(Candidate) + 'bin\64bit\obs64.exe');
end;

function ReadObsMajor(const Dir: string; var Major: Integer): Boolean;
var
  VersionText: string;
  FirstPart: string;
  DotPos: Integer;
begin
  Result := GetVersionNumbersString(
    AddBackslash(Dir) + 'bin\64bit\obs64.exe', VersionText);
  if Result then
  begin
    DotPos := Pos('.', VersionText);
    if DotPos > 0 then
      FirstPart := Copy(VersionText, 1, DotPos - 1)
    else
      FirstPart := VersionText;
    Major := StrToIntDef(FirstPart, 0);
    Result := Major > 0;
  end;
end;

function IsPortableObs(const Dir: string): Boolean;
var
  Root: string;
begin
  Root := AddBackslash(Dir);
  Result :=
    FileExists(Root + 'portable_mode') or
    FileExists(Root + 'portable_mode.txt') or
    FileExists(Root + 'obs_portable_mode') or
    FileExists(Root + 'obs_portable_mode.txt');
end;

function TryObsDirFromRegistry(): string;
var
  Candidate: string;
begin
  Result := '';
  Candidate := '';

  if RegQueryStringValue(HKLM64, 'SOFTWARE\OBS Studio', '', Candidate) and
     AcceptObsDir(Candidate) then
  begin
    Result := Candidate;
    Exit;
  end;

  if RegQueryStringValue(HKLM32, 'SOFTWARE\OBS Studio', '', Candidate) and
     AcceptObsDir(Candidate) then
  begin
    Result := Candidate;
    Exit;
  end;

  if RegQueryStringValue(HKCU, 'SOFTWARE\OBS Studio', '', Candidate) and
     AcceptObsDir(Candidate) then
  begin
    Result := Candidate;
    Exit;
  end;

  if RegQueryStringValue(HKLM64,
       'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio',
       'InstallLocation', Candidate) and AcceptObsDir(Candidate) then
  begin
    Result := Candidate;
    Exit;
  end;

  if RegQueryStringValue(HKLM32,
       'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio',
       'InstallLocation', Candidate) and AcceptObsDir(Candidate) then
  begin
    Result := Candidate;
    Exit;
  end;
end;

function GetCommandLineParam(const Name: string; var Value: string): Boolean;
var
  I: Integer;
  Prefix: string;
  Param: string;
begin
  Result := False;
  Prefix := Name + '=';
  for I := 1 to ParamCount do
  begin
    Param := ParamStr(I);
    if CompareText(Copy(Param, 1, Length(Prefix)), Prefix) = 0 then
    begin
      Value := Copy(Param, Length(Prefix) + 1,
                    Length(Param) - Length(Prefix));
      Result := True;
      Exit;
    end;
  end;
end;

function InitializeSetup(): Boolean;
var
  Override: string;
  DefaultDir: string;
begin
  Result := True;
  ObsDir := '';
  ObsMajor := 0;
  ObsVersionKnown := False;
  Override := '';
  DefaultDir := '';

  if GetCommandLineParam('/OBSDIR', Override) and AcceptObsDir(Override) then
    ObsDir := Override
  else
  begin
    ObsDir := TryObsDirFromRegistry();
    if ObsDir = '' then
    begin
      DefaultDir := ExpandConstant('{autopf}\obs-studio');
      if AcceptObsDir(DefaultDir) then
        ObsDir := DefaultDir;
    end;
  end;

  if ObsDir <> '' then
  begin
    ObsVersionKnown := ReadObsMajor(ObsDir, ObsMajor);
    if not ObsVersionKnown then
    begin
      { obs64.exe exists but carries no version resource (for example a custom
        build). Assume the modern layout instead of blocking the install. }
      ObsVersionKnown := True;
      ObsMajor := 33;
    end;
  end
  else if not WizardSilent then
    MsgBox(FmtMessage(CustomMessage('ObsNotFound'), [ExpandConstant(
             '{commonappdata}\obs-studio\plugins\obs-beauty-filter')]),
           mbInformation, MB_OK);
end;

function GetDefaultPluginDir(Param: string): string;
begin
  if (ObsDir <> '') and IsPortableObs(ObsDir) then
    Result := AddBackslash(ObsDir) + 'plugins\obs-beauty-filter'
  else
    Result := ExpandConstant(
      '{commonappdata}\obs-studio\plugins\obs-beauty-filter');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  { Hide the destination page when OBS was found: the installer knows the
    correct path. Portable users whose OBS is not registered still get the
    page and can point it at <OBS>\plugins\obs-beauty-filter. }
  Result := (PageID = wpSelectDir) and (ObsDir <> '');
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if ObsVersionKnown and (ObsMajor < 33) then
    Result := FmtMessage(CustomMessage('ObsTooOld'), [IntToStr(ObsMajor)]);
end;

function GetObsExePath(Param: string): string;
begin
  Result := AddBackslash(ObsDir) + 'bin\64bit\obs64.exe';
end;

function CanLaunchObs(): Boolean;
begin
  Result := (ObsDir <> '') and FileExists(GetObsExePath(''));
end;
