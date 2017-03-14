@echo off
if x%1==x exit

Setlocal

set ACTION=%1
set COMPILER=%2
set TARGET=%3
set SETENVBAT=%4
set ERR=%ACTION%_SDK_win_%COMPILER%_%TARGET%.err
set TARGET2=^|%TARGET%
call %SETENVBAT% >nul

rem msbuild and devenv are nearly equal here. But only devenv accepts %ENV_VARIABLE% in sln, msbuild needs us to expand them.
rem Call devenv rather than devenv.exe, it's necessary for reading stdout and exitcode.
rem Using one or the other is quite arbitrary, depends on tools one has installed, you can change it.

rem Expand environment variables in sln.
powershell.exe -command "(Get-Content 'src\Lightsprint.%COMPILER%.sln' ) | Foreach-Object { [Environment]::ExpandEnvironmentVariables($_) } | Set-Content 'src\Lightsprint.%COMPILER%.%TARGET%.sln'"

echo %ACTION%ing %COMPILER% "Debug DLL%TARGET2%"...
if     %COMPILER%==vs2010 devenv  src\Lightsprint.%COMPILER%.%TARGET%.sln /%ACTION% "Debug DLL%TARGET2%" >%ERR%
if NOT %COMPILER%==vs2010 msbuild src\Lightsprint.%COMPILER%.%TARGET%.sln /t:%ACTION% /p:Configuration="Debug DLL" /p:Platform=%TARGET% >%ERR%
if ERRORLEVEL 1 goto error

echo %ACTION%ing %COMPILER% "Release DLL%TARGET2%"...
if     %COMPILER%==vs2010 devenv  src\Lightsprint.%COMPILER%.%TARGET%.sln /%ACTION% "Release DLL%TARGET2%" >%ERR%
if NOT %COMPILER%==vs2010 msbuild src\Lightsprint.%COMPILER%.%TARGET%.sln /t:%ACTION% /p:Configuration="Release DLL" /p:Platform=%TARGET% >%ERR%
if ERRORLEVEL 1 goto error

rem Cleanup.
del "src\Lightsprint.%COMPILER%.%TARGET%.sln"

del %ERR%
echo OK, done.
Endlocal
goto:eof

:error
echo !!!!!!!!!!!!!!!!!!!!!!! ERROR (see %ERR%) !!!!!!!!!!!!!!!!!!!!!!!!!!!!
exit
rem now that builds are parallel, we build platforms serially. here we abort sequence of builds when one fails
