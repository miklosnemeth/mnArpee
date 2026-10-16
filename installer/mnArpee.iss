; Windows installer for the mnArpee VST3 (the Standalone is not included).
; Build it with Inno Setup 6 from a Release build, giving the version on the command line:
;   ISCC.exe /DMyAppVersion=1.0.0 installer\mnArpee.iss
; The result is build\installer\mnArpee-<version>-win-x64-setup.exe (see README "Installer").

#ifndef MyAppVersion
  #error Pass the version: ISCC.exe /DMyAppVersion=x.y.z installer\mnArpee.iss
#endif

#define MyAppName "mnArpee"
#define MyAppGuid "B3D7E1A4-5C28-4F96-A0E3-7D91C4B6F258"

[Setup]
; The same GUID as MyAppGuid above ({{ is the escape of a single brace). The old uninstaller is found with it.
AppId={{B3D7E1A4-5C28-4F96-A0E3-7D91C4B6F258}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=MNAudio
AppPublisherURL=https://github.com/miklosnemeth/mnArpee
AppSupportURL=https://github.com/miklosnemeth/mnArpee/issues
DefaultDirName={commoncf64}\MNAudio\mnArpee
; Always the folder above, also when an older version was installed somewhere else.
UsePreviousAppDir=no
; A DAW that has the plugin loaded is asked to close, so the files can be replaced.
CloseApplications=yes
CloseApplicationsFilter=*.vst3,*.exe,*.dll
RestartApplications=no
DefaultGroupName=MNAudio
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir=..\build\installer
OutputBaseFilename=mnArpee-{#MyAppVersion}-win-x64-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName={#MyAppName} {#MyAppVersion}
WizardStyle=modern

[Files]
; The plugin goes to the standard VST3 folder every host scans.
Source: "..\build\mnArpee_artefacts\Release\VST3\mnArpee.vst3\*"; DestDir: "{commoncf64}\VST3\mnArpee.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs
; Licenses and the readme sit in the install folder.
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Resources\Fonts\Montserrat-OFL.txt"; DestDir: "{app}\Licenses"; Flags: ignoreversion
Source: "..\Resources\Fonts\RobotoCondensed-LICENSE.txt"; DestDir: "{app}\Licenses"; Flags: ignoreversion

[InstallDelete]
; Older installers put the license and readme in Program Files.
Type: filesandordirs; Name: "{autopf}\MNAudio\mnArpee"

[Code]
// An older (or the same) version of mnArpee on this machine is found through the uninstall entry
// of Setup (same AppId). Setup tells which version it replaces, asks before it installs an older
// version over a newer one, and runs the old uninstaller before it installs.
var
  OldVersion: String;
  OldUninstaller: String;

function UninstallKey(): String;
begin
  Result := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{' + '{#MyAppGuid}' + '}_is1';
end;

function NextPart(var Version: String): Integer;
var
  Dot: Integer;
begin
  Dot := Pos('.', Version);
  if Dot = 0 then
  begin
    Result := StrToIntDef(Version, 0);
    Version := '';
  end
  else
  begin
    Result := StrToIntDef(Copy(Version, 1, Dot - 1), 0);
    Version := Copy(Version, Dot + 1, Length(Version));
  end;
end;

// -1, 0 or 1 when A is older than, the same as or newer than B.
function CompareVersions(A, B: String): Integer;
var
  X, Y: Integer;
begin
  Result := 0;
  while (Result = 0) and ((A <> '') or (B <> '')) do
  begin
    X := NextPart(A);
    Y := NextPart(B);
    if X < Y then
      Result := -1
    else if X > Y then
      Result := 1;
  end;
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
  OldVersion := '';
  OldUninstaller := '';
  if RegQueryStringValue(HKLM, UninstallKey(), 'DisplayVersion', OldVersion) then
  begin
    if RegQueryStringValue(HKLM, UninstallKey(), 'UninstallString', OldUninstaller) then
      OldUninstaller := RemoveQuotes(OldUninstaller);
    if CompareVersions(OldVersion, '{#MyAppVersion}') > 0 then
      // A silent install does not go back to an older version.
      if SuppressibleMsgBox('A newer version of {#MyAppName} (' + OldVersion + ') is installed.' + #13#10 +
                            'Install the older version {#MyAppVersion} over it anyway?',
                            mbConfirmation, MB_YESNO or MB_DEFBUTTON2, IDNO) = IDNO then
        Result := False;
  end;
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo, MemoComponentsInfo,
                         MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result := '';
  if OldVersion <> '' then
    Result := 'Version ' + OldVersion + ' is installed. It is removed first, then version {#MyAppVersion} is installed.' +
              NewLine + NewLine;
  Result := Result + MemoDirInfo;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode, Waited: Integer;
begin
  Result := '';
  if (OldVersion = '') or (OldUninstaller = '') or not FileExists(OldUninstaller) then
    Exit;

  Log('Running the uninstaller of version ' + OldVersion + ': ' + OldUninstaller);
  if Exec(OldUninstaller, '/VERYSILENT /NORESTART /SUPPRESSMSGBOXES', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
  begin
    Log('The old uninstaller returned ' + IntToStr(ResultCode));
    // Setup's uninstaller finishes in a second process: wait until its entry is gone (at most 20 s).
    Waited := 0;
    while RegKeyExists(HKLM, UninstallKey()) and (Waited < 20000) do
    begin
      Sleep(250);
      Waited := Waited + 250;
    end;
  end
  else
    // The files are replaced anyway.
    Log('The old uninstaller could not be started: ' + SysErrorMessage(ResultCode));
end;
