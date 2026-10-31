; Inno Setup script for Freddy Verb (Windows x64).
; Build: iscc /DVersion=0.1.0 /DBuildDir=..\..\build packaging\windows\FreddyVerb.iss
#ifndef Version
  #define Version "0.1.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build"
#endif
#define Art BuildDir + "\FreddyVerb_artefacts\Release"

[Setup]
AppId={{6E2F3C1A-4B6D-4E83-9A1F-3F7E5D2C9B01}
AppName=Freddy Verb
AppVersion={#Version}
AppPublisher=Freddy Audio
DefaultDirName={commonpf64}\Freddy Audio\Freddy Verb
DisableDirPage=yes
DefaultGroupName=Freddy Verb
OutputBaseFilename=Freddy Verb {#Version} Setup
OutputDir={#BuildDir}\installer
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2/max
SolidCompression=yes
LicenseFile=..\..\CREDITS.md

[Types]
Name: "full"; Description: "All formats"
Name: "custom"; Description: "Choose formats"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in"; Types: full custom
Name: "clap"; Description: "CLAP plug-in"; Types: full custom
Name: "aax"; Description: "AAX plug-in (Pro Tools)"; Types: full; Check: AaxBuilt
Name: "standalone"; Description: "Standalone application"; Types: full

[Files]
Source: "{#Art}\VST3\Freddy Verb.vst3\*"; DestDir: "{commoncf64}\VST3\Freddy Verb.vst3"; Components: vst3; Flags: recursesubdirs ignoreversion
Source: "{#Art}\CLAP\Freddy Verb.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
Source: "{#Art}\AAX\Freddy Verb.aaxplugin\*"; DestDir: "{commoncf64}\Avid\Audio\Plug-Ins\Freddy Verb.aaxplugin"; Components: aax; Flags: recursesubdirs ignoreversion skipifsourcedoesntexist
Source: "{#Art}\Standalone\Freddy Verb.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\Freddy Verb"; Filename: "{app}\Freddy Verb.exe"; Components: standalone

[Code]
function AaxBuilt: Boolean;
begin
  Result := DirExists(ExpandConstant('{#Art}\AAX\Freddy Verb.aaxplugin'));
end;
