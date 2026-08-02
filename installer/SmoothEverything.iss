#define AppName "SmoothEverything"
#define AppPublisher "SmoothEverything Project"
#define AppExeName "SmoothEverything.ControlPanel.exe"
#define EngineExeName "SmoothEverything.Engine.exe"
#define ProjectUrl "https://github.com/sakaman/SmoothEverything"

#ifndef AppVersion
  #define AppVersion "0.1.4"
#endif

#ifndef PublishSource
  #define PublishSource AddBackslash(SourcePath) + "..\artifacts\publish\win-x64"
#endif

#define BrandingIcon AddBackslash(SourcePath) + "..\assets\branding\SmoothEverything.ico"

[Setup]
AppId={{BDC9575D-80C5-49ED-BD5C-9F0C476390FF}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#ProjectUrl}
AppSupportURL={#ProjectUrl}/issues
AppUpdatesURL={#ProjectUrl}/releases
AppCopyright=Copyright (c) 2026 SmoothEverything contributors
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.19041
OutputDir=..\artifacts
OutputBaseFilename=SmoothEverything-Setup-{#AppVersion}-x64
SetupIconFile={#BrandingIcon}
UninstallDisplayIcon={app}\{#AppExeName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
CloseApplicationsFilter={#AppExeName},{#EngineExeName}
RestartApplications=no
SetupLogging=yes
ChangesAssociations=no
ChangesEnvironment=no
VersionInfoVersion={#AppVersion}.0
VersionInfoProductVersion={#AppVersion}.0
VersionInfoProductTextVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} installer
VersionInfoProductName={#AppName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "{#AddBackslash(SourcePath)}languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#PublishSource}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{userdesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

[CustomMessages]
english.AppStillRunning=SmoothEverything is still running. Close the control panel and tray engine, then try again.
chinesesimplified.AppStillRunning=SmoothEverything 仍在运行。请关闭设置窗口和托盘引擎后重试。

[Code]
const
  WM_CLOSE = $0010;
  ControlPanelWindowClass = 'SmoothEverything.NativeControlPanel';
  ControlPanelWindowTitle = 'SmoothEverything';
  EngineWindowClass = 'SmoothEverything.Engine.Window.v1';

function CloseWindowsByClassName(const ClassName: String): Boolean;
var
  Attempts: Integer;
  Window: HWND;
begin
  for Attempts := 0 to 49 do
  begin
    Window := FindWindowByClassName(ClassName);
    if Window = 0 then
    begin
      Result := True;
      Exit;
    end;
    PostMessage(Window, WM_CLOSE, 0, 0);
    Sleep(100);
  end;
  Result := FindWindowByClassName(ClassName) = 0;
end;

function CloseWindowsByTitle(const WindowTitle: String): Boolean;
var
  Attempts: Integer;
  Window: HWND;
begin
  for Attempts := 0 to 49 do
  begin
    Window := FindWindowByWindowName(WindowTitle);
    if Window = 0 then
    begin
      Result := True;
      Exit;
    end;
    PostMessage(Window, WM_CLOSE, 0, 0);
    Sleep(100);
  end;
  Result := FindWindowByWindowName(WindowTitle) = 0;
end;

function CloseSmoothEverything: Boolean;
var
  ControlPanelClosed: Boolean;
  EngineClosed: Boolean;
begin
  { Close the UI first so its reconnect timer cannot relaunch the engine. }
  ControlPanelClosed := CloseWindowsByTitle(ControlPanelWindowTitle);
  ControlPanelClosed :=
    CloseWindowsByClassName(ControlPanelWindowClass) and ControlPanelClosed;
  EngineClosed := CloseWindowsByClassName(EngineWindowClass);
  if ControlPanelClosed and EngineClosed then
    Sleep(500);
  Result := ControlPanelClosed and EngineClosed;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if not CloseSmoothEverything then
    Result := CustomMessage('AppStillRunning');
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    if not CloseSmoothEverything then
    begin
      SuppressibleMsgBox(
        CustomMessage('AppStillRunning'),
        mbError,
        MB_OK,
        IDOK);
      Abort;
    end;
    RegDeleteValue(
      HKCU,
      'Software\Microsoft\Windows\CurrentVersion\Run',
      'SmoothEverything');
  end;
end;
