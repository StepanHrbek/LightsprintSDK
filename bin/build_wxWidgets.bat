echo Running build threads in parallel...

for /f "usebackq delims=#" %%a in (`"%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -version [15.0^,16.0^) -property installationPath`) do set VS2017_PATH=%%a
for /f "usebackq delims=#" %%a in (`"%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -version [16.0^,17.0^) -property installationPath`) do set VS2019_PATH=%%a
for /f "usebackq delims=#" %%a in (`"%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -version [17.0^,18.0^) -property installationPath`) do set VS2022_PATH=%%a

start build_wxWidgets_2platforms vc141 "%VS2017_PATH%\VC\Auxiliary\Build\vcvars32.bat" "%VS2017_PATH%\VC\Auxiliary\Build\vcvars64.bat"
start build_wxWidgets_2platforms vc142 "%VS2019_PATH%\VC\Auxiliary\Build\vcvars32.bat" "%VS2019_PATH%\VC\Auxiliary\Build\vcvars64.bat"
start build_wxWidgets_2platforms vc143 "%VS2022_PATH%\VC\Auxiliary\Build\vcvars32.bat" "%VS2022_PATH%\VC\Auxiliary\Build\vcvars64.bat"
