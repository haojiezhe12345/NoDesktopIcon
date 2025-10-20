REG ADD "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID\{F5CD59F3-8E88-4324-A23D-01EA050EBF72}\InProcServer32" /ve /t REG_SZ /d "%~dp0ExplorerNoDesktopIcons.dll" /f
REG ADD "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID\{F5CD59F3-8E88-4324-A23D-01EA050EBF72}\InProcServer32" /v "ThreadingModel" /t REG_SZ /d "Apartment" /f

REG ADD "HKEY_LOCAL_MACHINE\Software\Classes\Drive\shellex\FolderExtensions\{F5CD59F3-8E88-4324-A23D-01EA050EBF72}" /ve /t REG_SZ /d "ExplorerNoDesktopIcons" /f
REG ADD "HKEY_LOCAL_MACHINE\Software\Classes\Drive\shellex\FolderExtensions\{F5CD59F3-8E88-4324-A23D-01EA050EBF72}" /v "DriveMask" /t REG_DWORD /d 0x000000ff /f

pause
