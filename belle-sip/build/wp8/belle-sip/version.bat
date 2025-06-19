@ECHO off

FOR /F "delims=" %%a IN ('findstr /B AC_INIT ..\..\..\configure.ac') DO (
	FOR /F "tokens=1,2,3 delims=[,]" %%1 IN ("%%a") DO (
		ECHO #define PACKAGE_VERSION "%%3" > config.h
	)
)
