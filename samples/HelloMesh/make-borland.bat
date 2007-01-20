@cd ..\..\3rd\borland
@call build-libs.bat
@cd ..\..\samples\HelloMesh
@set path=%path%;c:\borland\bcc55\bin
@bcc32.exe -ps -DNDEBUG -IC:\Borland\BCC55\Include -I..\..\include -LC:\Borland\BCC55\Lib ..\..\lib\win32-borland\RRMesh.lib HelloMesh.cpp
@rem C:\Borland\BCC55\bin\bcc32.exe -ps -DNDEBUG -IC:\Borland\BCC55\Include -LC:\Borland\BCC55\Lib HelloMesh.cpp h:\c\rr\lib\win32-borland\RRMesh.lib
@rem C:\Borland\BCC55\bin\bcc32.exe -O2 -DNDEBUG -VM -IC:\Borland\BCC55\Include -LC:\Borland\BCC55\Lib HelloMesh.cpp RRMesh.lib
