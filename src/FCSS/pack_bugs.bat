rd /q /s distro
mkdir distro
mkdir distro\shaders
copy ..\..\data\shaders\ubershader.* distro\shaders
mkdir distro\maps
copy ..\..\data\maps\rrbugs_*.tga distro\maps
copy ..\..\data\maps\spot?.tga distro\maps
mkdir distro\3ds
mkdir distro\3ds\koupelna
copy ..\..\data\3ds\koupelna\*.* distro\3ds\koupelna
mkdir distro\3ds\sponza
copy ..\..\data\3ds\sponza\*.3ds distro\3ds\sponza
copy ..\..\data\3ds\sponza\*.tga distro\3ds\sponza
copy bugs\*.* distro
mkdir distro\images
copy bugs\images\*.* distro\images
copy ..\..\3rd\glut\glut32.dll distro
copy ..\..\bin\fcss.exe distro\run.exe
call -min upx distro\run.exe
del rrbugs.zip
cd distro
rem 7za a -r -tzip -mx=9 ..\RRBugs *.*
c:\progra~1\winrar\rar a -s -r -m5 -md4096 ..\RRBugs *.*