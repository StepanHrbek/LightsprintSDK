@echo off

set WX=%WXWIDGETS_DIR%
if a%WX%==a (
	echo First set WXWIDGETS_DIR environment variable to your wxWidgets root, e.g. C:\wxWidgets. No spaces please.
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

set WX=%WXWIDGETS_DIR%
set COMPILER=%1
set BITS=%2
set SETENVBAT=%3
set ERR=%COMPILER%_log
set CPU=
if x%BITS%==x64 set CPU=_amd64
cd %WX%\build\msw
if exist %SETENVBAT% (
	call %SETENVBAT% >%ERR%

	echo Building wxWidgets %COMPILER% %BITS%bit Debug static...
	nmake -f makefile.vc USE_OPENGL=1 COMPILER_PREFIX=%COMPILER%%CPU% BUILD=debug >%ERR% 2>&1
	if ERRORLEVEL 1 goto error

	echo Building wxWidgets %COMPILER% %BITS%bit Release static ...
	nmake -f makefile.vc USE_OPENGL=1 COMPILER_PREFIX=%COMPILER%%CPU% BUILD=release >%ERR% 2>&1
	if ERRORLEVEL 1 goto error
) else (
	echo VS%COMPILER% %BITS%bit not found.
)

echo OK, done.
goto end
:error
echo !!!!!!!!!!!!!!!!!!!!!!! ERROR (see %WX%\build\msw\%ERR%) !!!!!!!!!!!!!!!!!!!!!!!!!!!!
:end
