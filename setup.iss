; =============================================================
; PowerBox installer script - Inno Setup 6
; Keep this file ASCII-only to avoid any encoding issues.
; UI language is handled by lang\*.ini inside the app.
; Place next to the .iss:  x64\Release\  = folder to package
; =============================================================

#define AppName "PowerBox"
; Version source of truth for the installer. Keep it in sync with the GitHub
; release tag. You can override on the command line instead of editing here:
;   ISCC.exe /DAppVersion=1.2.0 setup.iss
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define AppPublisher "PowerBox"
#define AppExeName "PowerBox.exe"
; Build output folder, relative to this .iss location.
#define BuildDir "x64\Release"

[Setup]
AppId={{7F0A3D9C-1E2B-4C3D-8A9B-1A2B3C4D5E6F}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} v{#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExeName}
OutputDir={#BuildDir}
OutputBaseFilename=PowerBoxSetup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
VersionInfoVersion={#AppVersion}
CloseApplications=force

[Files]
Source: "{#BuildDir}\{#AppExeName}";         DestDir: "{app}"
Source: "{#BuildDir}\WebView2Loader.dll";    DestDir: "{app}"
Source: "{#BuildDir}\lang\*";                DestDir: "{app}\lang"; Flags: recursesubdirs
Source: "{#BuildDir}\res\*";                 DestDir: "{app}\res";  Flags: recursesubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppName}";  Filename: "{app}\{#AppExeName}"

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Run {#AppName} now"; Flags: nowait postinstall skipifsilent

[Code]
const
  WebView2EvergreenGuid = '{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}';

function HasWebView2Client(const RootKey: Integer; const SubKey: string): Boolean;
var
  Value: string;
begin
  Result := RegKeyExists(RootKey, SubKey);
  if Result then
    Result := RegValueExists(RootKey, SubKey, 'pv');
  if Result then
    Result := RegQueryStringValue(RootKey, SubKey, 'pv', Value) and (Value <> '');
end;

function IsWebView2RuntimeInstalled(): Boolean;
begin
  Result := HasWebView2Client(HKCU, 'Software\Microsoft\EdgeUpdate\Clients\' + WebView2EvergreenGuid);
  if Result then Exit;

  Result := HasWebView2Client(HKLM, 'Software\Microsoft\EdgeUpdate\Clients\' + WebView2EvergreenGuid);
  if Result then Exit;

  Result := HasWebView2Client(HKLM, 'Software\WOW6432Node\Microsoft\EdgeUpdate\Clients\' + WebView2EvergreenGuid);
end;

function InitializeSetup(): Boolean;
var
  ResultCode: Integer;
begin
  Result := True;
  if IsWebView2RuntimeInstalled() then
    Exit;

  if MsgBox('PowerBox uses the Microsoft Edge WebView2 Runtime, which was not detected on this computer.' #13#13 'Open the official WebView2 Runtime download page and install it, then run this installer again?', mbError, MB_YESNO) = IDYES then
  begin
    ShellExec('open', 'https://developer.microsoft.com/microsoft-edge/webview2/', '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
  end;

  Result := False;
end;
