mkdir distro
mkdir distro\shaders
copy shaders distro\shaders
mkdir distro\koupelna
copy koupelna\koupelna3.3ds distro\koupelna
copy koupelna\*.tga distro\koupelna
mkdir distro\sponza
copy sponza\sponza.3ds distro\sponza
copy sponza\*.tga distro\sponza
copy readme.txt distro
copy spot0.tga distro
copy glut32.dll distro
copy view*.bat distro
copy ..\..\bin\fcss.exe distro\rrview.exe
-min upx distro\rrview.exe
cd distro
c:\progra~1\winrar\rar a -s -r ..\rrview *.*