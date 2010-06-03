@echo off
if x%1==x exit

set ACTION=%1
set COMPILER=%2
set TARGET=%3
set SETENVBAT=%4
set ERR=%ACTION%_SDK_win_%COMPILER%_%TARGET%.err
if NOT x%TARGET%==x- set TARGET2=^|%TARGET%
call %SETENVBAT% >nul

echo %ACTION%ing %COMPILER% "Debug DLL%TARGET2%"...
rem Nesmim volat primo devenv.exe, nedostal bych z nej stdout a exitcode.
devenv src\RR.%COMPILER%.sln /%ACTION% "Debug DLL%TARGET2%" >%ERR%
if ERRORLEVEL 1 goto error

echo %ACTION%ing %COMPILER% "Release DLL%TARGET2%"...
devenv src\RR.%COMPILER%.sln /%ACTION% "Release DLL%TARGET2%" >%ERR%
if ERRORLEVEL 1 goto error

if %COMPILER%==vs2008 if %TARGET%==win32 (
	echo %ACTION%ing %COMPILER% "Shipping DLL%TARGET2%"...
	devenv src\RR.%COMPILER%.sln /%ACTION% "Shipping DLL%TARGET2%"" >%ERR%
	if ERRORLEVEL 1 goto error
)

del %ERR%
echo OK, done.
exit

:error
echo !!!!!!!!!!!!!!!!!!!!!!! ERROR (see %ERR%) !!!!!!!!!!!!!!!!!!!!!!!!!!!!
