; Lumen installer - GPL-3.0-only
#ifndef AppVersion
  #define AppVersion "1.4.0"
#endif
#ifndef PayloadDir
  #define PayloadDir "..\dist\payload"
#endif
#ifndef OutputPath
  #define OutputPath "..\dist"
#endif
#ifndef VersionMS
  #define VersionMS 65540
#endif
#ifndef VersionLS
  #define VersionLS 0
#endif

[Setup]
AppId={{1F0181CF-851F-4E47-9252-FF53703882AC}
AppName=Lumen
AppVersion={#AppVersion}
AppPublisher=AuronNetwork
AppPublisherURL=https://github.com/AuronNetwork
AppSupportURL=https://github.com/AuronNetwork/Lumen/issues
AppUpdatesURL=https://github.com/AuronNetwork/Lumen/releases
DefaultDirName={localappdata}\Programs\Lumen
DefaultGroupName=Lumen
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.19041
LicenseFile={#PayloadDir}\LICENSE
OutputDir={#OutputPath}
OutputBaseFilename=Lumen-Setup
SetupIconFile={#PayloadDir}\assets\lumen.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern dark
WizardSizePercent=110
CloseApplications=no
RestartApplications=no
AlwaysRestart=no
SetupMutex=Local\AuronNetwork.Lumen.Setup
UninstallDisplayIcon={app}\Lumen.exe
VersionInfoVersion={#AppVersion}
VersionInfoCompany=AuronNetwork
VersionInfoDescription=Lumen Setup
ChangesAssociations=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[INI]
Filename: "{app}\lumen-install.ini"; Section: "Install"; Key: "Product"; String: "Lumen"; Flags: uninsdeleteentry
Filename: "{app}\lumen-install.ini"; Section: "Install"; Key: "Repository"; String: "AuronNetwork/Lumen"; Flags: uninsdeleteentry
Filename: "{app}\lumen-install.ini"; Section: "Install"; Key: "Version"; String: "{#AppVersion}"; Flags: uninsdeleteentry

[UninstallDelete]
Type: files; Name: "{app}\lumen-install.ini"

[Icons]
Name: "{group}\Lumen"; Filename: "{app}\Lumen.exe"
Name: "{autodesktop}\Lumen"; Filename: "{app}\Lumen.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Lumen.exe"; Description: "Open Lumen"; Flags: nowait postinstall skipifsilent; Check: not IsAutoUpdate
Filename: "{app}\Lumen.exe"; Flags: nowait; Check: IsAutoUpdate

[Code]
function OpenProcess(Access: LongWord; Inherit: Boolean; ProcessId: LongWord): THandle;
  external 'OpenProcess@kernel32.dll stdcall';
function WaitForSingleObject(Handle: THandle; Milliseconds: LongWord): LongWord;
  external 'WaitForSingleObject@kernel32.dll stdcall';
function CloseHandle(Handle: THandle): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';
function CreateFile(Name: string; Access, Sharing: LongWord; Security: THandle;
  Creation, Flags: LongWord; Template: THandle): THandle;
  external 'CreateFileW@kernel32.dll stdcall';

function IsAutoUpdate: Boolean;
begin
  Result := ExpandConstant('{param:LUMENAUTOUPDATE|0}') = '1';
end;

function InitializeSetup: Boolean;
var ParentId: Integer; Parent: THandle; WaitResult: LongWord;
begin
  Result := True;
  if IsAutoUpdate then begin
    ParentId := StrToIntDef(ExpandConstant('{param:LUMENWAITPID|0}'), 0);
    if ParentId <= 0 then begin Result := False; Exit; end;
    Parent := OpenProcess($00100000, False, ParentId);
    if Parent <> 0 then begin
      WaitResult := WaitForSingleObject(Parent, 30000);
      CloseHandle(Parent);
      Result := WaitResult = 0;
      if not Result then Log('Lumen launcher did not exit in time; update cancelled.');
    end;
  end;
end;

function FileAvailable(const Name: string): Boolean;
var Handle: THandle;
begin
  Result := True;
  if not FileExists(Name) then Exit;
  Handle := CreateFile(Name, $40000000, 0, 0, 3, $80, 0);
  Result := Handle <> THandle(-1);
  if Result then CloseHandle(Handle);
end;

function FilesAvailable: Boolean;
begin
  Result := FileAvailable(ExpandConstant('{app}\Lumen.exe')) and
    FileAvailable(ExpandConstant('{app}\Lumen.dll')) and
    FileAvailable(ExpandConstant('{app}\assets\Geist-Regular.ttf')) and
    FileAvailable(ExpandConstant('{app}\assets\Geist-SemiBold.ttf'));
end;

function PrepareToInstall(var NeedsRestart: Boolean): string;
var VersionMS, VersionLS: Cardinal;
begin
  Result := '';
  if CheckForMutexes('Local\AuronNetwork.Lumen.Launcher') or not FilesAvailable then
    Result := 'Lumen files are in use or not writable. Save and close Minecraft and Lumen, then try again. Setup will not close the game.';
  if GetVersionNumbers(ExpandConstant('{app}\Lumen.exe'), VersionMS, VersionLS) then begin
    if (VersionMS > {#VersionMS}) or
       ((VersionMS = {#VersionMS}) and
        (VersionLS > {#VersionLS})) then
      Result := 'A newer version of Lumen is already installed. Use the current release.';
  end;
end;

function InitializeUninstall: Boolean;
begin
  Result := not CheckForMutexes('Local\AuronNetwork.Lumen.Launcher') and FilesAvailable;
  if not Result then MsgBox('Save and close Minecraft and Lumen before uninstalling. Your Lumen settings will be kept.', mbError, MB_OK);
end;
