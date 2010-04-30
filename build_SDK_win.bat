@echo off

start build_SDK_win_vs2003.bat
start build_SDK_win_vs2005_32.bat
start build_SDK_win_vs2005_64.bat
start build_SDK_win_vs2008_32.bat
start build_SDK_win_vs2008_64.bat

goto end





set PATH0=%PATH%
set ERR=build_SDK_win.err

call "%VS71COMNTOOLS%\vsvars32.bat" >nul

echo Building VS2003 Debug DLL...
rem Schvalne nebuilduju Lightsprint.sln urceny pro zakazniky ale privatni RR.sln obsahujici navic mimo jine rsa.lib
devenv src\RR.vs2003.sln /build "Debug DLL" >%ERR%
if ERRORLEVEL 1 goto error
echo Building VS2003 Release DLL...
devenv src\RR.vs2003.sln /build "Release DLL" >%ERR%
if ERRORLEVEL 1 goto error
       
set PATH=%PATH0%
call "%VS80COMNTOOLS%\vsvars32.bat" >%ERR%

echo Building VS2005 Debug DLL/Win32...
devenv src\RR.vs2005.sln /build "Debug DLL|Win32" >%ERR%
if ERRORLEVEL 1 goto error
echo Building VS2005 Release DLL/Win32...
devenv src\RR.vs2005.sln /build "Release DLL|Win32" >%ERR%
if ERRORLEVEL 1 goto error

echo Building VS2005 Debug DLL/x64...
devenv src\RR.vs2005.sln /build "Debug DLL|x64" >%ERR%
if ERRORLEVEL 1 goto error
echo Building VS2005 Release DLL/x64...
devenv src\RR.vs2005.sln /build "Release DLL|x64" >%ERR%
if ERRORLEVEL 1 goto error

set PATH=%PATH0%
call "%VS90COMNTOOLS%\vsvars32.bat" >nul

echo Building VS2008 Debug DLL/Win32...
devenv src\RR.vs2008.sln /build "Debug DLL|Win32" >%ERR%
if ERRORLEVEL 1 goto error
echo Building VS2008 Release DLL/Win32...
devenv src\RR.vs2008.sln /build "Release DLL|Win32" >%ERR%
if ERRORLEVEL 1 goto error
echo Building VS2008 Shipping DLL/Win32...
devenv src\RR.vs2008.sln /build "Shipping DLL|Win32" >%ERR%
if ERRORLEVEL 1 goto error

echo Building VS2008 Debug DLL/x64...
devenv src\RR.vs2008.sln /build "Debug DLL|x64" >%ERR%
if ERRORLEVEL 1 goto error
echo Building VS2008 Release DLL/x64...
devenv src\RR.vs2008.sln /build "Release DLL|x64" >%ERR%
if ERRORLEVEL 1 goto error

del %ERR%
echo SDK/win built OK.
goto end

:error
echo !!!!!!!!!!!!!!!!!!!!!!! ERROR (see %ERR%) !!!!!!!!!!!!!!!!!!!!!!!!!!!!

:end
