@echo off

start build_SDK_win_platform.bat Clean vs2010 win32 "%VS100COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat Clean vs2010 x64   "%vs100comntools%..\..\vc\bin\amd64\vcvars64.bat"
start build_SDK_win_platform.bat Clean vs2012 win32 "%VS110COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat Clean vs2012 x64   "%vs110comntools%..\..\vc\bin\x86_amd64\vcvarsx86_amd64.bat"
