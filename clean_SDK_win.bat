@echo off

call "%VS71COMNTOOLS%\vsvars32.bat" >nul
if ERRORLEVEL 1 goto error
devenv src\RR.vs2003.sln /clean "Debug DLL" >nul
if ERRORLEVEL 1 goto error
devenv src\RR.vs2003.sln /clean "Release DLL" >nul
if ERRORLEVEL 1 goto error

call "%VS80COMNTOOLS%\vsvars32.bat" >nul
if ERRORLEVEL 1 goto error
devenv src\RR.vs2005.sln /clean "Debug DLL|Win32" >nul
if ERRORLEVEL 1 goto error
devenv src\RR.vs2005.sln /clean "Release DLL|Win32" >nul
if ERRORLEVEL 1 goto error
devenv src\RR.vs2005.sln /clean "Debug DLL|x64" >nul
if ERRORLEVEL 1 goto error
devenv src\RR.vs2005.sln /clean "Release DLL|x64" >nul
if ERRORLEVEL 1 goto error

call "%VS90COMNTOOLS%\vsvars32.bat" >nul
if ERRORLEVEL 1 goto error
devenv src\RR.vs2008.sln /clean "Debug DLL|Win32" >nul
if ERRORLEVEL 1 goto error
devenv src\RR.vs2008.sln /clean "Release DLL|Win32" >nul
if ERRORLEVEL 1 goto error
devenv src\RR.vs2008.sln /clean "Shipping DLL|Win32" >nul
if ERRORLEVEL 1 goto error
devenv src\RR.vs2008.sln /clean "Debug DLL|x64" >nul
if ERRORLEVEL 1 goto error
devenv src\RR.vs2008.sln /clean "Release DLL|x64" >nul
if ERRORLEVEL 1 goto error

echo Clean OK.
goto end

:error
echo Chyba!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

:end