rem Asi kdyz neni nainstalovane MSDN, hhc.exe nemuze najit hha.dll. Mozna by stacilo misto MSDN nainstalovat jen http://go.microsoft.com/fwlink/?LinkId=14188

!doxygen\doxygen >log
!doxygen\hhc html\index.hhp
move /y html\index.chm ..\Lightsprint.chm
start ..\Lightsprint.chm
rd /s /q html