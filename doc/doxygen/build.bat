@echo off
rem Attempt to generate html documentation.

rem Locate doxygen. Give instructions and end if not found.
set DOXYGEN="C:\Program Files\doxygen\bin\doxygen.exe"
if not exist %DOXYGEN% (
    echo Doxygen not found, install it to default location
    echo {e.g. with commandline: winget install doxygen}
    echo or tweak location in this .bat if you already have it.
    exit
)

rem Delete old documentation.
rem This is necessary to detect situation, when some file (image)
rem is no longer copied there, but it still sits there
rem from previous builds and looks super good.
if exist ..\html (
    del /q ..\html\*.*
)

rem Generate html.
%DOXYGEN% >nul 2>doxy_warnings.txt

rem If there are warnings/errors, show them and end.
for /f %%i in ("doxy_warnings.txt") do set SIZE=%%~zi
if %SIZE% gtr 0 (
    start doxy_warnings.txt
    exit
)
del doxy_warnings.txt

rem Show only namespaces on page namespaces.
powershell -Command "(gc ../html/namespaces.html) -replace 'initResizable\(\)', 'initResizable(); toggleLevel(1)' | Out-File -encoding ASCII ../html/namespaces.html"

rem Make sections font smaller.
powershell -Command "(Add-Content ../html/doxygen-awesome.css 'h1 { font-size:130%%; }' )

rem Open generated html.
start ..\html\index.html
