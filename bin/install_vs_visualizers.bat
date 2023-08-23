rem Run this as administrator to add Lightsprint SDK visualizers into Visual Studio 2012, 2013, 2015, 2017, 2019, 2022.

for /f "usebackq delims=#" %%a in (`"%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -version [15.0^,16.0^) -property installationPath`) do set VS2017_PATH=%%a
for /f "usebackq delims=#" %%a in (`"%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -version [16.0^,17.0^) -property installationPath`) do set VS2019_PATH=%%a
for /f "usebackq delims=#" %%a in (`"%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -version [17.0^,18.0^) -property installationPath`) do set VS2022_PATH=%%a

copy ..\doc\Lightsprint.SDK.natvis "%VS110COMNTOOLS%/../Packages/Debugger/Visualizers"
copy ..\doc\Lightsprint.SDK.natvis "%VS120COMNTOOLS%/../Packages/Debugger/Visualizers"
copy ..\doc\Lightsprint.SDK.natvis "%VS140COMNTOOLS%/../Packages/Debugger/Visualizers"
copy ..\doc\Lightsprint.SDK.natvis "%VS2017_PATH%/Common7/Packages/Debugger/Visualizers"
copy ..\doc\Lightsprint.SDK.natvis "%VS2019_PATH%/Common7/Packages/Debugger/Visualizers"
copy ..\doc\Lightsprint.SDK.natvis "%VS2022_PATH%/Common7/Packages/Debugger/Visualizers"
