; Rabbit installer (Inno Setup 6)
;
; Build the payload first, then compile this script:
;   xmake build -j2 rabbit_App
;   (windeployqt + gtkwave + libusb into dist\ -- see BUILD_WINDOWS_RABBIT.md)
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /DAppVersion=1.2.0 installer\rabbit.iss
;
; The CI release workflow does exactly the same on the windows runner.
;
; AppId must stay constant: it is what Windows uses to recognise upgrades and
; the uninstaller.  Change it only if you want a *different* product.

#define AppName "Rabbit"
#ifndef AppVersion
  #define AppVersion "1.2.0"
#endif
#define AppPublisher "SiwuHC"
#define AppExe "rabbit_App.exe"
; dist\ is created by the build (windeployqt, gtkwave, libusb dll ...)
#define DistDir "..\dist"

[Setup]
AppId={{01401AB2-B0DE-5948-97E3-4B3FB48422F2}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
; per-user install: no UAC prompt.  Use PrivilegesRequired=admin if you want a
; machine-wide install instead.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\installer_out
OutputBaseFilename=Rabbit-{#AppVersion}-windows-x86_64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#AppExe}
; the repository has no LICENSE file yet; use one if it appears
#if FileExists("..\LICENSE")
LicenseFile=..\LICENSE
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Files]
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "Start {#AppName}"; Flags: nowait postinstall skipifsilent

[Code]
// The board talks through libusb, which on Windows needs the device to use the
// WinUSB driver (Zadig).  Mention it on the last page instead of failing later.
function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo,
                         MemoTypeInfo, MemoComponentsInfo, MemoGroupInfo,
                         MemoTasksInfo: String): String;
begin
  Result := MemoDirInfo + NewLine + NewLine + MemoTasksInfo + NewLine + NewLine +
            'Note: the FPGA board needs the WinUSB driver.  If Rabbit cannot see' + NewLine +
            'the board, install it for the device with Zadig.' + NewLine;
end;
