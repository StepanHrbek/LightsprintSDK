@echo off
set sdkpath=.
set assimppath=c:\Assimp

xcopy /y "%assimppath%\code\*.cpp"              "%sdkpath%\code\"
xcopy /y "%assimppath%\code\*.h"                "%sdkpath%\code\"
xcopy /y "%assimppath%\code\*.inl"              "%sdkpath%\code\"
xcopy /y "%assimppath%\contrib\ConvertUTF\C*.*" "%sdkpath%\contrib\ConvertUTF\"
xcopy /y "%assimppath%\contrib\irrXML\*.*"      "%sdkpath%\contrib\irrXML\"
xcopy /y "%assimppath%\contrib\unzip\*.*"       "%sdkpath%\contrib\unzip\"
xcopy /y "%assimppath%\contrib\zlib\*.*"        "%sdkpath%\contrib\zlib\"
xcopy /y "%assimppath%\include\*.*"             "%sdkpath%\include\"
xcopy /y "%assimppath%\include\Compiler\*.*"    "%sdkpath%\include\Compiler\"


echo Now replace #include "../revision.h" in AssimpPCH.cpp with #define SVNRevision 826
