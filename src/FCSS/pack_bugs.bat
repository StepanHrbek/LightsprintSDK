rd /q /s distro
mkdir distro
mkdir distro\shaders
copy ..\..\data\shaders\ubershader.* distro\shaders
mkdir distro\maps
copy ..\..\data\maps\bug?.tga distro\maps
copy ..\..\data\maps\spot?.tga distro\maps
mkdir distro\3ds
mkdir distro\3ds\koupelna
copy ..\..\data\3ds\koupelna4\*.3ds distro\3ds\koupelna
copy ..\..\data\3ds\koupelna4\*.tga distro\3ds\koupelna
rem mkdir distro\3ds\sponza
rem copy ..\..\data\3ds\sponza\*.3ds distro\3ds\sponza
rem copy ..\..\data\3ds\sponza\*.tga distro\3ds\sponza
rem mkdir distro\3ds\sibenik
rem copy ..\..\data\3ds\sibenik\*.3ds distro\3ds\sibenik
rem copy ..\..\data\3ds\sibenik\*.tga distro\3ds\sibenik
copy bugs\contact.txt distro
copy bugs\game.txt distro
copy bugs\usage.txt distro
copy ..\..\3rd\glut\glut32.dll distro
copy bugs\play_koupelna3.bat distro
copy bugs\play_koupelna4.bat distro
copy bugs\play_koupelna5.bat distro
rem copy view_sponza.bat distro
copy ..\..\bin\fcss.exe distro\run.exe
rem copy "C:\Program Files\Microsoft Visual Studio 8\VC\redist\x86\Microsoft.VC80.CRT\*.*" distro
rem copy "C:\Program Files\Microsoft Visual Studio 8\VC\redist\x86\Microsoft.VC80.OPENMP\*.*" distro
call -min upx distro\run.exe
del rrbugs.rar
cd distro
c:\progra~1\winrar\rar a -s -r -m5 -md4096 ..\rrbugs *.*