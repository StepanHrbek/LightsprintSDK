@rem Generate html
!doxygen\doxygen >log

@rem Show only namespaces on page namespaces.
powershell -Command "(gc ../html/namespaces.html) -replace 'initResizable\(\)', 'initResizable(); toggleLevel(1)' | Out-File -encoding ASCII ../html/namespaces.html"

@rem Make sections font smaller.
powershell -Command "(Add-Content ../html/doxygen-awesome.css 'h1 { font-size:130%%; }' )

start ..\html\index.html
exit

rem Asi kdyz neni nainstalovane MSDN, hhc.exe nemuze najit hha.dll. Mozna by stacilo misto MSDN nainstalovat jen http://go.microsoft.com/fwlink/?LinkId=14188
!doxygen\hhc html\index.hhp
move /y html\index.chm ..\Lightsprint.chm
start ..\Lightsprint.chm
rd /s /q html