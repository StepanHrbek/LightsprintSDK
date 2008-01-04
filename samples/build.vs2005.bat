@echo off

call "%VS80COMNTOOLS%\vsvars32.bat" >nul

echo Building VS2005 Debug DLL samples...
devenv samples.vs2005.sln /build "Debug DLL" >log
if ERRORLEVEL 1 goto error
echo Building VS2005 Release DLL samples...
devenv samples.vs2005.sln /build "Release DLL" >log
if ERRORLEVEL 1 goto error

echo OK.
exit

:error
echo Error (see log)!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
