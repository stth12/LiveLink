#define Root "..\.."
[Setup]
AppId={{903E3D6B-4FE6-4B19-B69B-48C57C5B43FC}
AppName=Live Link
AppVersion=0.1.0
AppPublisher=Live Link
DefaultDirName={autopf}\Live Link
DefaultGroupName=Live Link
OutputDir={#Root}\dist
OutputBaseFilename=LiveLinkSetup
SetupIconFile={#Root}\installer\assets\LiveLink.ico
LicenseFile={#Root}\LICENSE
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0.19041
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
WizardImageFile={#Root}\installer\assets\Wizard.bmp
WizardSmallImageFile={#Root}\installer\assets\WizardSmall.bmp
UninstallDisplayIcon={app}\Live Link.exe
CloseApplications=yes
RestartApplications=no

[Types]
Name: "full"; Description: "Complete Live Link system"
Name: "custom"; Description: "Custom installation"; Flags: iscustom
[Components]
Name: "desktop"; Description: "Desktop controller"; Types: full custom
Name: "service"; Description: "Required audio background process"; Types: full custom; Flags: fixed
Name: "plugin"; Description: "64-bit VST3 plugin"; Types: full custom
[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked; Components: desktop
Name: "startup"; Description: "Start the background process with Windows (current user)"; Flags: unchecked
[Files]
Source: "{#Root}\dist\windows\Live Link.exe"; DestDir: "{app}"; Components: desktop; Flags: ignoreversion
Source: "{#Root}\dist\windows\Live Link Service.exe"; DestDir: "{app}"; Components: service; Flags: ignoreversion
Source: "{#Root}\dist\windows\Live Link.vst3\*"; DestDir: "{commoncf64}\VST3\Live Link.vst3"; Components: plugin; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\COPYING"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\third_party\JUCE-LICENSE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "StartLiveLink.vbs"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{group}\Live Link"; Filename: "{app}\Live Link.exe"; Components: desktop
Name: "{autodesktop}\Live Link"; Filename: "{app}\Live Link.exe"; Tasks: desktopicon; Components: desktop
[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "LiveLink"; ValueData: """{sys}\wscript.exe"" ""{app}\StartLiveLink.vbs"""; Tasks: startup; Flags: uninsdeletevalue
[Run]
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall add rule name=""Live Link Control"" dir=in action=allow program=""{app}\Live Link Service.exe"" protocol=TCP localport=49320 profile=private remoteip=localsubnet"; Flags: runhidden waituntilterminated
Filename: "{app}\Live Link.exe"; Description: "Launch Live Link"; Components: desktop; Flags: postinstall nowait skipifsilent runasoriginaluser
[UninstallRun]
Filename: "{app}\Live Link Service.exe"; Parameters: "--stop"; Flags: runhidden waituntilterminated; RunOnceId: "StopLiveLink"
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""Live Link Control"" program=""{app}\Live Link Service.exe"""; Flags: runhidden waituntilterminated; RunOnceId: "RemoveLiveLinkFirewall"
[Code]
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectComponents) and ((not WizardIsComponentSelected('desktop')) or (not WizardIsComponentSelected('plugin'))) then
    Result := MsgBox('Streaming requires the background process, a VST3 plugin and an iOS receiver. The desktop controller is needed to view the pairing PIN. Continue with the selected components?', mbConfirmation, MB_YESNO) = IDYES;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usPostUninstall) and (not UninstallSilent) then
    if MsgBox('Retain Live Link preferences and diagnostic logs?', mbConfirmation, MB_YESNO) = IDNO then begin
      DeleteFile(ExpandConstant('{userappdata}\Live Link\preferences.json'));
      DeleteFile(ExpandConstant('{userappdata}\Live Link\diagnostics.log'));
      RemoveDir(ExpandConstant('{userappdata}\Live Link'));
    end;
end;
