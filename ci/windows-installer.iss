[Setup]
AppId=RAMDStudios.RAMDPDF
AppName=RAMD PDF
AppVersion=0.9.0
AppPublisher=RAMD Studios
DefaultDirName={localappdata}\Programs\RAMD PDF
DefaultGroupName=RAMD PDF
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\installer
OutputBaseFilename=RAMD-PDF-0.9.0-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
SetupIconFile=..\assets\ramd-pdf.ico
UninstallDisplayIcon={app}\ramd-pdf.exe

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked

[Icons]
Name: "{group}\RAMD PDF"; Filename: "{app}\ramd-pdf.exe"
Name: "{autodesktop}\RAMD PDF"; Filename: "{app}\ramd-pdf.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ramd-pdf.exe"; Description: "Open RAMD PDF"; Flags: nowait postinstall skipifsilent
