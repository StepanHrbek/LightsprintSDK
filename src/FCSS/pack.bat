rd /q /s distro
mkdir distro
mkdir distro\shaders
copy shaders distro\shaders
mkdir distro\koupelna4
copy ..\..\data\3ds\koupelna4\koupelna4.3ds distro\koupelna4
copy ..\..\data\3ds\koupelna4\*.tga distro\koupelna4
mkdir distro\sponza
copy ..\..\data\3ds\sponza\sponza.3ds distro\sponza
copy ..\..\data\3ds\sponza\*.tga distro\sponza
copy readme.txt distro
copy spot0.tga distro
copy glut32.dll distro
copy view*.bat distro
copy ..\..\bin\fcss.exe distro\rrview.exe
rem copy "C:\Program Files\Microsoft Visual Studio 8\VC\redist\x86\Microsoft.VC80.CRT\*.*" distro
rem copy "C:\Program Files\Microsoft Visual Studio 8\VC\redist\x86\Microsoft.VC80.OPENMP\*.*" distro
call -min upx distro\rrview.exe
del rrview.rar
cd distro
c:\progra~1\winrar\rar a -s -r -m5 -md4096 ..\rrview *.*