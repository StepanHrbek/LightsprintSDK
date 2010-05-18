@echo off

start build_SDK_win_platform.bat Build vs2003 -     "%VS71COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat Build vs2005 win32 "%VS80COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat Build vs2005 x64   "%vs80comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_SDK_win_platform.bat Build vs2008 win32 "%VS90COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat Build vs2008 x64   "%vs90comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_SDK_win_platform.bat Build vs2010 win32 "%VS100COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat Build vs2010 x64   "%vs100comntools%..\..\vc\bin\amd64\vcvars64.bat"
