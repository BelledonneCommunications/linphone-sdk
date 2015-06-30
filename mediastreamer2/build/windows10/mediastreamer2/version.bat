@ECHO off

SET gitlog=
FOR /f "delims=" %%a IN ('git log -1 "--pretty=format:%%H" ../../../configure.ac') DO SET gitlog=%%a

IF [%gitlog%] == [] GOTO UnknownGitVersion

FOR /f "delims=" %%a IN ('git describe --always') DO SET gitdescribe=%%a
GOTO End

:UnknownGitVersion
SET gitdescribe=unknown

:End
ECHO #define GIT_VERSION "%gitdescribe%" > gitversion.h


FOR /F "delims=" %%a IN ('findstr /B AC_INIT ..\..\..\configure.ac') DO (
	FOR /F "tokens=1,2,3 delims=[,]" %%1 IN ("%%a") DO (
		ECHO #define MEDIASTREAMER_VERSION "%%3" > mediastreamer-config.h
	)
)
