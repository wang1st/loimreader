; Inno Setup Script for LoimReader (Qt6, dynamic deploy)
; Requires: Inno Setup 6+

#ifndef AppVersion
#define AppVersion "2.7.2"
#endif

[Setup]
AppId={{1E8D784C-0E64-4B8E-A414-LOIMREADER-APP}}
AppName=LoimReader
AppVerName=影谷长图阅读器 {#AppVersion}
AppVersion={#AppVersion}
AppPublisher=LoimReader
AppPublisherURL=https://loimreader.com
DefaultDirName={autopf}\LoimReader
DefaultGroupName=LoimReader
DisableProgramGroupPage=yes
OutputDir=dist
OutputBaseFilename=LoimReader-Setup-v{#AppVersion}
SetupIconFile=icons\windows.ico
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayIcon={app}\bin\LoimReader.exe

[Languages]
Name: "schinese"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Files]
Source: "dist\bin\*"; DestDir: "{app}\bin"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{autoprograms}\影谷长图阅读器"; Filename: "{app}\bin\LoimReader.exe"; WorkingDir: "{app}\bin"
Name: "{autodesktop}\影谷长图阅读器"; Filename: "{app}\bin\LoimReader.exe"; WorkingDir: "{app}\bin"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加选项:"; Flags: unchecked

[Run]
Filename: "{app}\bin\LoimReader.exe"; Description: "运行 影谷长图阅读器"; Flags: nowait postinstall skipifsilent


