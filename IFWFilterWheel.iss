; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "IFW Optec Filter wheel X2 Driver"
#define MyAppVersion "1.0"
#define MyAppPublisher "RTI-Zone"
#define MyAppURL "https://rti-zone.org"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{D48FFC0B-716A-4D2D-BD65-D64013FE81EF}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={code:TSXInstallDir}\Resources\Common
DefaultGroupName={#MyAppName}

; Need to customise these
; First is where you want the installer to end up
OutputDir=installer
; Next is the name of the installer
OutputBaseFilename=IFWFilterWheel_X2_Installer
; Final one is the icon you would like on the installer. Comment out if not needed.
SetupIconFile=rti_zone_logo.ico
Compression=lzma
SolidCompression=yes
; We don't want users to be able to select the drectory since read by TSXInstallDir below
DisableDirPage=yes
; Uncomment this if you don't want an uninstaller.
;Uninstallable=no
CloseApplications=yes
DirExistsWarning=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Dirs]
Name: "{app}\Plugins\FilterWheelPlugIns";
Name: "{app}\Plugins64\FilterWheelPlugIns";

[Files]
Source: "filterwheellist optec.txt";                                DestDir: "{app}\Miscellaneous Files"; Flags: ignoreversion
Source: "filterwheellist optec.txt";                                DestDir: "{app}\Miscellaneous Files"; DestName: "filterwheellist64 optec.txt";Flags: ignoreversion
; 32 bit
Source: "IFW.ui";                                                   DestDir: "{app}\Plugins\FilterWheelPlugIns"; Flags: ignoreversion
Source: "libIFWFilterWheel\Win32\Release\libIFWFilterWheel.dll";    DestDir: "{app}\Plugins\FilterWheelPlugIns"; Flags: ignoreversion
; 64 bits
Source: "IFW.ui";                                                   DestDir: "{app}\Plugins64\FilterWheelPlugIns"; Flags: ignoreversion; Check: DirExists(ExpandConstant('{app}\Plugins64\FilterWheelPlugIns'))
Source: "libIFWFilterWheel\x64\Release\libIFWFilterWheel.dll";      DestDir: "{app}\Plugins64\FilterWheelPlugIns"; Flags: ignoreversion; Check: DirExists(ExpandConstant('{app}\Plugins64\FilterWheelPlugIns'))

[Code]
{* Below is a function to read TheSkyXInstallPath.txt and confirm that the directory does exist
   This is then used in the DefaultDirName above
   *}
var
  Location: String;
  LoadResult: Boolean;

function TSXInstallDir(Param: String) : String;
begin
  LoadResult := LoadStringFromFile(ExpandConstant('{userdocs}') + '\Software Bisque\TheSkyX Professional Edition\TheSkyXInstallPath.txt', Location);
  { Check that could open the file}
  if not LoadResult then
    RaiseException('Unable to find the installation path for The Sky X');
  {Check that the file exists}
  if not DirExists(Location) then
    RaiseException('The SkyX installation directory ' + Location + ' does not exist');
  Result := Location;
end;
