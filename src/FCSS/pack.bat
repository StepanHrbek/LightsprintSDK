rd /q /s distro
mkdir distro
mkdir distro\shaders
copy ..\..\data\shaders\ubershader.* distro\shaders
mkdir distro\maps
copy ..\..\data\maps\spot?.tga distro\maps
mkdir distro\3ds
mkdir distro\3ds\koupelna
copy ..\..\data\3ds\koupelna4\*.3ds distro\3ds\koupelna
copy ..\..\data\3ds\koupelna4\*.tga distro\3ds\koupelna
mkdir distro\3ds\sponza
copy ..\..\data\3ds\sponza\*.3ds distro\3ds\sponza
copy ..\..\data\3ds\sponza\*.tga distro\3ds\sponza
mkdir distro\3ds\sibenik
copy ..\..\data\3ds\sibenik\*.3ds distro\3ds\sibenik
copy ..\..\data\3ds\sibenik\*.tga distro\3ds\sibenik
copy readme.txt distro
copy glut32.dll distro
copy view_*.bat distro
copy ..\..\bin\fcss.exe distro\rrview.exe
rem copy "C:\Program Files\Microsoft Visual Studio 8\VC\redist\x86\Microsoft.VC80.CRT\*.*" distro
rem copy "C:\Program Files\Microsoft Visual Studio 8\VC\redist\x86\Microsoft.VC80.OPENMP\*.*" distro
call -min upx distro\rrview.exe
del rrview.rar
cd distro
c:\progra~1\winrar\rar a -s -r -m5 -md4096 ..\rrview *.*