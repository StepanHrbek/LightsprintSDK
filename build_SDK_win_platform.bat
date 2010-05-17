@echo off
set COMPILER=%1
set TARGET=%2
set SETENVBAT=%3
set ERR=build_SDK_win_%COMPILER%_%TARGET%.err
if NOT x%TARGET%==x- set TARGET2=|%TARGET%
call %SETENVBAT% >nul

echo Building %COMPILER% Debug DLL%TARGET2%...
devenv src\RR.%COMPILER%.sln /build "Debug DLL%TARGET2%" >%ERR%
if ERRORLEVEL 1 goto error

echo Building %COMPILER% Release DLL%TARGET2%...
devenv src\RR.%COMPILER%.sln /build "Release DLL%TARGET2%" >%ERR%
if ERRORLEVEL 1 goto error

if %COMPILER%==vs2008 if %TARGET%==win32 (
	echo Building %COMPILER% Shipping DLL%TARGET2%"...
	devenv src\RR.%COMPILER%.sln /build "Shipping DLL%TARGET2%"" >%ERR%
	if ERRORLEVEL 1 goto error
)

del %ERR%
echo SDK/win built OK.
goto end

:error
echo !!!!!!!!!!!!!!!!!!!!!!! ERROR (see %ERR%) !!!!!!!!!!!!!!!!!!!!!!!!!!!!

:end
