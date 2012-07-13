@echo off
set sdkpath=.
set assimppath=c:\c\Assimp

xcopy /y    "%assimppath%\code\*.cpp"                  "%sdkpath%\code\"
xcopy /y    "%assimppath%\code\*.h"                    "%sdkpath%\code\"
xcopy /y    "%assimppath%\code\*.hpp"                  "%sdkpath%\code\"
xcopy /y    "%assimppath%\code\*.inl"                  "%sdkpath%\code\"
xcopy /y    "%assimppath%\contrib\clipper\*.*"         "%sdkpath%\contrib\clipper\"
xcopy /y    "%assimppath%\contrib\ConvertUTF\C*.*"     "%sdkpath%\contrib\ConvertUTF\"
xcopy /y    "%assimppath%\contrib\irrXML\*.*"          "%sdkpath%\contrib\irrXML\"
xcopy /y /s "%assimppath%\contrib\poly2tri\poly2tri"   "%sdkpath%\contrib\poly2tri\poly2tri\"
xcopy /y    "%assimppath%\contrib\unzip\*.*"           "%sdkpath%\contrib\unzip\"
xcopy /y    "%assimppath%\contrib\zlib\*.*"            "%sdkpath%\contrib\zlib\"
xcopy /y    "%assimppath%\include\assimp\*.*"          "%sdkpath%\include\assimp\"
xcopy /y    "%assimppath%\include\assimp\Compiler\*.*" "%sdkpath%\include\assimp\Compiler\"


echo Consider updating number in revision.h
