@echo off
set ACTION=%1
if x%1==x set ACTION=Build

start build_SDK_win_platform.bat %ACTION% vs2003 -     "%VS71COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat %ACTION% vs2005 win32 "%VS80COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat %ACTION% vs2005 x64   "%vs80comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_SDK_win_platform.bat %ACTION% vs2008 win32 "%VS90COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat %ACTION% vs2008 x64   "%vs90comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_SDK_win_platform.bat %ACTION% vs2010 win32 "%VS100COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat %ACTION% vs2010 x64   "%vs100comntools%..\..\vc\bin\amd64\vcvars64.bat"
