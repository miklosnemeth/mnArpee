; Windows installer for the mnArpee VST3 (the Standalone is not included).
; Build it with Inno Setup 6 from a Release build, giving the version on the command line:
;   ISCC.exe /DMyAppVersion=1.0.0 installer\mnArpee.iss
; The result is build\installer\mnArpee-<version>-win-x64-setup.exe (see README "Installer").

#ifndef MyAppVersion
  #error Pass the version: ISCC.exe /DMyAppVersion=x.y.z installer\mnArpee.iss
#endif

#define MyAppName "mnArpee"

[Setup]
AppId={{B3D7E1A4-5C28-4F96-A0E3-7D91C4B6F258}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=MNAudio
AppPublisherURL=https://github.com/miklosnemeth/mnArpee
AppSupportURL=https://github.com/miklosnemeth/mnArpee/issues
DefaultDirName={commoncf64}\MNAudio\mnArpee
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
