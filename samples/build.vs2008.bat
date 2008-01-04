@echo off

call "%VS90COMNTOOLS%\vsvars32.bat" >nul

echo Building VS2008 Debug DLL samples...
devenv samples.vs2008.sln /build "Debug DLL" >log
if ERRORLEVEL 1 goto error
echo Building VS2008 Release DLL samples...
devenv samples.vs2008.sln /build "Release DLL" >log
if ERRORLEVEL 1 goto error

echo OK.
exit

:error
echo Error (see log)!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
