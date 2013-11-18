REM define the tools
SET grep_exe=grep.exe
SET sed_exe=sed.exe

REM get the version
FOR /F %%i IN (' %grep_exe% "AC_INIT(" ../../../configure.ac ^| %sed_exe% -e "s/.*belle-sip\]//" ^| %sed_exe% -e "s/].*//" ^| %sed_exe% -e "s/.*\[//" ') DO SET version=%%i

REM set the version in the props file
REM %sed_exe% -i.bak "s/BELLESIP_PACKAGE_VERSION>\"[0-9.]*\"</BELLESIP_PACKAGE_VERSION>\"%version%\"</g" belle-sip.props
