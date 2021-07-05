echo Running 4 build threads in parallel...

for /f "usebackq delims=#" %%a in (`"%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -version [15.0^,16.0^) -property installationPath`) do set VS2017_PATH=%%a
for /f "usebackq delims=#" %%a in (`"%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -version [16.0^,17.0^) -property installationPath`) do set VS2019_PATH=%%a

start build_wxWidgets_2platforms vc100 "%vs100comntools%vsvars32.bat" "%vs100comntools%..\..\vc\bin\amd64\vcvars64.bat"
start build_wxWidgets_2platforms vc110 "%vs110comntools%vsvars32.bat" "%vs110comntools%..\..\vc\bin\x86_amd64\vcvarsx86_amd64.bat"
start build_wxWidgets_2platforms vc120 "%vs120comntools%vsvars32.bat" "%vs120comntools%..\..\vc\bin\x86_amd64\vcvarsx86_amd64.bat"
start build_wxWidgets_2platforms vc140 "%vs140comntools%vsvars32.bat" "%vs140comntools%..\..\vc\bin\x86_amd64\vcvarsx86_amd64.bat"
start build_wxWidgets_2platforms vc141 "%VS2017_PATH%\VC\Auxiliary\Build\vcvars32.bat" "%VS2017_PATH%\VC\Auxiliary\Build\vcvars64.bat"
start build_wxWidgets_2platforms vc142 "%VS2019_PATH%\VC\Auxiliary\Build\vcvars32.bat" "%VS2019_PATH%\VC\Auxiliary\Build\vcvars64.bat"

exit

building 32bit and 64bit in parallel causes conflicts, so we can't run 7 threads in parallel as shown here:

start build_wxWidgets_platform vc71  32 "%vs71comntools%vsvars32.bat"
start build_wxWidgets_platform vc80  32 "%vs80comntools%vsvars32.bat"
start build_wxWidgets_platform vc80  64 "%vs80comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_wxWidgets_platform vc90  32 "%vs90comntools%vsvars32.bat"
start build_wxWidgets_platform vc90  64 "%vs90comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_wxWidgets_platform vc100 32 "%vs100comntools%vsvars32.bat"
start build_wxWidgets_platform vc100 64 "%vs100comntools%..\..\vc\bin\amd64\vcvars64.bat"
