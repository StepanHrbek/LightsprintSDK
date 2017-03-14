@echo off

rem Build platforms serially.
rem It is experiment, now that single platform build is parallel (<MultiProcessorCompilation>true</MultiProcessorCompilation> in src/configs/rr.props).
rem It detects compile problems faster, but total time of good build might be bit worse.
call build_SDK_win_platform.bat Build vs2015 x64   "%vs140comntools%..\..\vc\bin\x86_amd64\vcvarsx86_amd64.bat"
call build_SDK_win_platform.bat Build vs2015 win32 "%VS140COMNTOOLS%\vsvars32.bat"
call build_SDK_win_platform.bat Build vs2013 x64   "%vs120comntools%..\..\vc\bin\x86_amd64\vcvarsx86_amd64.bat"
call build_SDK_win_platform.bat Build vs2013 win32 "%VS120COMNTOOLS%\vsvars32.bat"
call build_SDK_win_platform.bat Build vs2012 x64   "%vs110comntools%..\..\vc\bin\x86_amd64\vcvarsx86_amd64.bat"
call build_SDK_win_platform.bat Build vs2012 win32 "%VS110COMNTOOLS%\vsvars32.bat"
call build_SDK_win_platform.bat Build vs2010 x64   "%vs100comntools%..\..\vc\bin\amd64\vcvars64.bat"
call build_SDK_win_platform.bat Build vs2010 win32 "%VS100COMNTOOLS%\vsvars32.bat"
exit


rem Build platforms in parallel.
start build_SDK_win_platform.bat Build vs2010 win32 "%VS100COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat Build vs2010 x64   "%vs100comntools%..\..\vc\bin\amd64\vcvars64.bat"
start build_SDK_win_platform.bat Build vs2012 win32 "%VS110COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat Build vs2012 x64   "%vs110comntools%..\..\vc\bin\x86_amd64\vcvarsx86_amd64.bat"
start build_SDK_win_platform.bat Build vs2013 win32 "%VS120COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat Build vs2013 x64   "%vs120comntools%..\..\vc\bin\x86_amd64\vcvarsx86_amd64.bat"
start build_SDK_win_platform.bat Build vs2015 win32 "%VS140COMNTOOLS%\vsvars32.bat"
start build_SDK_win_platform.bat Build vs2015 x64   "%vs140comntools%..\..\vc\bin\x86_amd64\vcvarsx86_amd64.bat"
