set build_type=Release
if "x%1" == "xdebug" set build_type=Debug
set linphone_builder_path=%cd%
set forward_linphone_builder_path=%linphone_builder_path:\=/%
set errfile=%linphone_builder_path%\WORK\error.log
set wrnfile=%linphone_builder_path%\WORK\warning.log
set logfile=%linphone_builder_path%\WORK\build.log

set program_files_x86=%ProgramFiles(x86)%
if not "%program_files_x86%" == "" goto win64
set program_files_x86=%ProgramFiles%
:win64

if NOT DEFINED old_path set old_path=%PATH%
set PATH=%PATH%;%linphone_builder_path%\WORK\windows_tools

rmdir /S /Q WORK
rmdir /S /Q OUTPUT

md WORK\cmake-python
cd WORK\cmake-python

cmake.exe ../.. -G "Visual Studio 9 2008" -DCMAKE_BUILD_TYPE=%build_type% -DCMAKE_PREFIX_PATH=%forward_linphone_builder_path%/OUTPUT -DCMAKE_INSTALL_PREFIX=%forward_linphone_builder_path%/OUTPUT -DLINPHONE_BUILDER_CONFIG_FILE=configs/config-python.cmake

cd ..\..

set PATH=%old_path%

"%program_files_x86%\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" "%linphone_builder_path%\WORK\cmake-python\Project.sln" "%build_type%|Win32" /M%NUMBER_OF_PROCESSORS% /errfile:%errfile% /wrnfile:%wrnfile% /logfile:%logfile%
