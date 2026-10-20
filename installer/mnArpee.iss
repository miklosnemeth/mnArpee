; Windows installer for the mnArpee VST3 (the Standalone is not included).
; It is built by the normal Release build (the CMake target mnArpeeInstaller, when Inno Setup 6
; is installed). By hand, from a Release build, giving the version on the command line:
;   ISCC.exe /DMyAppVersion=1.0.0 installer\mnArpee.iss
; The result is build\installer\mnArpee-<version>-win-x64-setup.exe (see README "Installer").
; What it shares with the mnKord installer (the folders, closing the DAW, the old version) is in
; MnAudioInstaller.iss, the same file in both repositories.

#ifndef MyAppVersion
  #error Pass the version: ISCC.exe /DMyAppVersion=x.y.z installer\mnArpee.iss
#endif

; The built VST3 bundle and the output folder; CMake passes them, these are the defaults.
#ifndef Vst3Bundle
  #define Vst3Bundle "..\build\mnArpee_artefacts\Release\VST3\mnArpee.vst3"
#endif
#ifndef InstallerOutputDir
  #define InstallerOutputDir "..\build\installer"
#endif

#define MyAppName "mnArpee"
#define MyAppGuid "B3D7E1A4-5C28-4F96-A0E3-7D91C4B6F258"
#define MyRepo "mnArpee"
#include "MnAudioInstaller.iss"

[Setup]
; The same GUID as MyAppGuid ({{ is the escape of a single brace). The old uninstaller is found with it.
AppId={{{#MyAppGuid}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
OutputDir={#InstallerOutputDir}

[Files]
; The plugin goes to the standard VST3 folder every host scans, in a folder of its own for MNAudio.
Source: "{#Vst3Bundle}\*"; DestDir: "{#MyVst3Dest}"; \
    Flags: ignoreversion recursesubdirs createallsubdirs
; Licenses and the readme sit in the install folder.
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Resources\Fonts\Montserrat-OFL.txt"; DestDir: "{app}\Licenses"; Flags: ignoreversion
Source: "..\Resources\Fonts\RobotoCondensed-LICENSE.txt"; DestDir: "{app}\Licenses"; Flags: ignoreversion
; The user guide (English and Hungarian), with Start menu shortcuts.
Source: "..\docs\mnArpee-User-Guide.pdf"; DestDir: "{app}\Docs"; Flags: ignoreversion
Source: "..\docs\mnArpee-Felhasznaloi-utmutato.pdf"; DestDir: "{app}\Docs"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\MNAudio\mnArpee user guide"; Filename: "{app}\Docs\mnArpee-User-Guide.pdf"
Name: "{autoprograms}\MNAudio\mnArpee felhasznaloi utmutato"; Filename: "{app}\Docs\mnArpee-Felhasznaloi-utmutato.pdf"
