mkdir distro
mkdir distro\shaders
copy shaders distro\shaders
mkdir distro\koupelna4
copy koupelna4\koupelna4.3ds distro\koupelna4
copy koupelna4\*.tga distro\koupelna4
mkdir distro\sponza
copy sponza\sponza.3ds distro\sponza
copy sponza\*.tga distro\sponza
copy readme.txt distro
copy spot0.tga distro
copy glut32.dll distro
copy view*.bat distro
copy ..\..\bin\fcss.exe distro\rrview.exe
call -min upx distro\rrview.exe
del rrview.rar
cd distro
c:\progra~1\winrar\rar a -s -r ..\rrview *.*