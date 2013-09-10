REM define the tools
SET grep_exe=grep.exe
SET sed_exe=sed.exe

REM get the version
SET grep_cmd=%grep_exe% "" ../../../VERSION
FOR /F %%i IN (' %grep_cmd% ') DO SET version=%%i

REM set the version in the props file
%sed_exe% -i.bak "s/BELLESIP_PACKAGE_VERSION>\"[0-9.]*\"</BELLESIP_PACKAGE_VERSION>\"%version%\"</g" belle-sip.props
