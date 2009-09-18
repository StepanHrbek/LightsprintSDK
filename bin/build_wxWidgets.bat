@echo off

echo Building wxWidgets for Windows...
set WX=%WXWIDGETS_DIR%
if a%WX%==a (
	echo First set WXWIDGETS_DIR environment variable to your wxWidgets root, e.g. C:\wxWidgets-2.9. No spaces please.
	exit
)
if not exist %WX%\build\msw\wx_vc9.sln (
	echo wxWidgets not found at %WXWIDGETS_DIR%, fix your WXWIDGETS_DIR environment variable.
	exit
)
if not exist %WX%\include\wx\msw\setup.h (
	echo Using default setup. You can break now, tweak setup in %WX%\include\wx\msw\setup.h and rerun.
	copy %WX%\include\wx\msw\setup0.h %WX%\include\wx\msw\setup.h
)
set ERR=wxwidgets_build.err
del %ERR% 2>nul
set path_backup=%path%

echo Creating temporary projects in %WX% ...
cd %WX%\build
mkdir msw32 2>nul
mkdir msw64 2>nul
cd msw
for %%a IN (wx_vc7*) DO sed -e 's/vc_/vc2003_32_/g' %%a > ..\msw32\%%a
for %%a IN (wx_vc8*) DO sed -e 's/vc_/vc2005_32_/g' %%a > ..\msw32\%%a
for %%a IN (wx_vc9*) DO sed -e 's/vc_/vc2008_32_/g' %%a > ..\msw32\%%a
for %%a IN (wx_vc8*) DO sed -e 's/vc_/vc2005_64_/g;s/Win32/x64/g' %%a > ..\msw64\%%a
for %%a IN (wx_vc9*) DO sed -e 's/vc_/vc2008_64_/g;s/Win32/x64/g' %%a > ..\msw64\%%a


if exist "%VS71COMNTOOLS%\vsvars32.bat" (
	call "%VS71COMNTOOLS%\vsvars32.bat" >%ERR%

	echo Rebuilding VS2003 Win32 Debug static ...
	devenv %WX%\build\msw32\wx_vc7.sln /clean "Debug" >%ERR%
	devenv %WX%\build\msw32\wx_vc7.sln /build "Debug" >%ERR%
	devenv %WX%\build\msw32\wx_vc7.sln /build "Debug" >%ERR%
	devenv %WX%\build\msw32\wx_vc7.sln /build "Debug" >%ERR%
	devenv %WX%\build\msw32\wx_vc7.sln /build "Debug" >%ERR%
	devenv %WX%\build\msw32\wx_vc7.sln /build "Debug" >%ERR%
	if ERRORLEVEL 1 goto error

	echo Rebuilding VS2003 Win32 Release static ...
	devenv %WX%\build\msw32\wx_vc7.sln /clean "Release" >%ERR%
	devenv %WX%\build\msw32\wx_vc7.sln /build "Release" >%ERR%
	devenv %WX%\build\msw32\wx_vc7.sln /build "Release" >%ERR%
	devenv %WX%\build\msw32\wx_vc7.sln /build "Release" >%ERR%
	devenv %WX%\build\msw32\wx_vc7.sln /build "Release" >%ERR%
	devenv %WX%\build\msw32\wx_vc7.sln /build "Release" >%ERR%
	if ERRORLEVEL 1 goto error
) else (
	echo VS2003 not found.
)
rem Must be outside if( ) because path_backup may contain )
set path=%path_backup%


if exist "%VS80COMNTOOLS%\vsvars32.bat" (
	call "%VS80COMNTOOLS%\vsvars32.bat" >%ERR%

	echo Rebuilding VS2005 Win32 Debug static ...
	devenv %WX%\build\msw32\wx_vc8.sln /clean "Debug|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc8.sln /build "Debug|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc8.sln /build "Debug|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc8.sln /build "Debug|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc8.sln /build "Debug|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc8.sln /build "Debug|Win32" >%ERR%
	if ERRORLEVEL 1 goto error

	echo Rebuilding VS2005 Win32 Release static ...
	devenv %WX%\build\msw32\wx_vc8.sln /clean "Release|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc8.sln /build "Release|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc8.sln /build "Release|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc8.sln /build "Release|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc8.sln /build "Release|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc8.sln /build "Release|Win32" >%ERR%
	if ERRORLEVEL 1 goto error

	echo Rebuilding VS2005 x64 Debug static ...
	devenv %WX%\build\msw64\wx_vc8.sln /clean "Debug|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc8.sln /build "Debug|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc8.sln /build "Debug|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc8.sln /build "Debug|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc8.sln /build "Debug|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc8.sln /build "Debug|x64" >%ERR%
	if ERRORLEVEL 1 goto error

	echo Rebuilding VS2005 x64 Release static ...
	devenv %WX%\build\msw64\wx_vc8.sln /clean "Release|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc8.sln /build "Release|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc8.sln /build "Release|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc8.sln /build "Release|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc8.sln /build "Release|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc8.sln /build "Release|x64" >%ERR%
	if ERRORLEVEL 1 goto error
) else (
	echo VS2005 not found.
)
set path=%path_backup%


if exist "%VS90COMNTOOLS%\vsvars32.bat" (
	call "%VS90COMNTOOLS%\vsvars32.bat" >%ERR%

	echo Rebuilding VS2008 Win32 Debug static ...
	devenv %WX%\build\msw32\wx_vc9.sln /clean "Debug|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc9.sln /build "Debug|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc9.sln /build "Debug|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc9.sln /build "Debug|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc9.sln /build "Debug|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc9.sln /build "Debug|Win32" >%ERR%
	if ERRORLEVEL 1 goto error

	echo Rebuilding VS2008 Win32 Release static ...
	devenv %WX%\build\msw32\wx_vc9.sln /clean "Release|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc9.sln /build "Release|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc9.sln /build "Release|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc9.sln /build "Release|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc9.sln /build "Release|Win32" >%ERR%
	devenv %WX%\build\msw32\wx_vc9.sln /build "Release|Win32" >%ERR%
	if ERRORLEVEL 1 goto error

	echo Rebuilding VS2008 x64 Debug static ...
	devenv %WX%\build\msw64\wx_vc9.sln /clean "Debug|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc9.sln /build "Debug|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc9.sln /build "Debug|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc9.sln /build "Debug|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc9.sln /build "Debug|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc9.sln /build "Debug|x64" >%ERR%
	if ERRORLEVEL 1 goto error

	echo Rebuilding VS2008 x64 Release static ...
	devenv %WX%\build\msw64\wx_vc9.sln /clean "Release|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc9.sln /build "Release|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc9.sln /build "Release|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc9.sln /build "Release|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc9.sln /build "Release|x64" >%ERR%
	devenv %WX%\build\msw64\wx_vc9.sln /build "Release|x64" >%ERR%
	if ERRORLEVEL 1 goto error
) else (
	echo VS2008 not found.
)
set path=%path_backup%


echo DONE, all libraries built OK. You can save space by deleting %WX%\build\msw32 and msw64.
del %ERR% 2>nul
goto end
:error
echo !!!!!!!!!!!!!!!!!!!!!!! ERROR (see %WX%\%ERR%) !!!!!!!!!!!!!!!!!!!!!!!!!!!!
:end
