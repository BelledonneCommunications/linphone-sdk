echo off
if [%1]==[] goto usage


chdir > chdir_src.temp
set /p SRC_DIR=<chdir_src.temp
echo on
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\VsDevCmd.bat"
echo "MSVC command line environnement loaded."
cd %1
chdir > chdir_sdk.temp
set /p OUTPUT_SDK_BUILD=<chdir_sdk.temp
cd %SRC_DIR%\cmake\Windows\wrapper\
chdir > chdir_wrapper.temp
set /p OUTPUT_WRAPPER_BUILD=<chdir_wrapper.temp
echo "Building CsWrapper..."
msbuild -t:restore CsWrapper.csproj
msbuild CsWrapper.csproj /p:MDILCompile=true /p:Platform="x86" /t:build /p:Configuration=Release /p:OutputSdkBuild=%OUTPUT_SDK_BUILD%
cd ..\nuget
git describe > describe.temp
set /p DESCRIBE=<describe.temp
echo "Building nuget with version %DESCRIBE%"
msbuild NuGetLinphoneSDK.vcxproj /p:VersionNumber=%DESCRIBE% /p:OutputSdkBuild=%OUTPUT_SDK_BUILD% /p:OutputWrapperBuild=%OUTPUT_WRAPPER_BUILD%
cd %SRC_DIR%
move /y "cmake\Windows\nuget\LinphoneSDK*.nupkg" "%1\linphone-sdk\desktop"
exit /B 0

:usage
echo off
@echo Usage: %0 ^<build directory^>
@echo This script must be run from the top-level source directory of linphone-sdk project.
exit /B 1


