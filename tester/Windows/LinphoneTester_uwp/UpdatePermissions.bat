
powershell -command "(Get-AppxPackage -Name "*LinphoneTester-uwp*").PublisherId" > publisherId.txt
set /p PUBLISHER_ID=<publisherId.txt
del publisherId.txt
echo "here :"
echo %PUBLISHER_ID%

REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\appointments\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\bluetoothSync\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\broadFileSystemAccess\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\cellularData\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\chat\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\contacts\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\gazeInput\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\location\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\microphone\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\phoneCall\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\phoneCallHistory\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\picturesLibrary\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\userAccountInformation\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\userDataTasks\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\userNotificationListener\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\videosLibrary\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f
REG ADD "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\webcam\LinphoneTester-uwp_%PUBLISHER_ID%" /v Value /t REG_SZ /d "Allow" /f