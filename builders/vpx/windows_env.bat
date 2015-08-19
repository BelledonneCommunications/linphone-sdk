
@if "%1" == "14" goto vs14
@if "%1" == "12" goto vs12
@if "%1" == "11" goto vs11
@if "%1" == "10" goto vs10
@if "%1" == "9" goto vs9
@goto end

:vs14
@call "%VS140COMNTOOLS%vsvars32.bat"
@goto printenv

:vs12
@call "%VS120COMNTOOLS%vsvars32.bat"
@goto printenv

:vs11
@call "%VS110COMNTOOLS%vsvars32.bat"
@goto printenv

:vs10
@call "%VS100COMNTOOLS%vsvars32.bat"
@goto printenv

:vs9
@call "%VS90COMNTOOLS%vsvars32.bat"
@goto printenv

:printenv

@echo %PATH% > windowsenv_path.txt
@echo %INCLUDE% > windowsenv_include.txt
@echo %LIB% > windowsenv_lib.txt
@echo %LIBPATH% > windowsenv_libpath.txt

:end
