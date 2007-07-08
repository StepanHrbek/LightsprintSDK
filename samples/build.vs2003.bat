@echo off

call "%VS71COMNTOOLS%\vsvars32.bat" >nul

echo Building VS2003 Debug DLL samples...
devenv samples.vs2003.sln /build "Debug DLL" >log
if ERRORLEVEL 1 goto error
echo Building VS2003 Release DLL samples...
devenv samples.vs2003.sln /build "Release DLL" >log
if ERRORLEVEL 1 goto error

echo OK.
exit

:error
echo Error (see log)!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
