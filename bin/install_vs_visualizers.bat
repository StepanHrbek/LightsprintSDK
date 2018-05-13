rem Run this as administrator to add Lightsprint SDK visualizers into Visual Studio 2012, 2013, 2015.

copy ..\doc\Lightsprint.SDK.natvis "%VS110COMNTOOLS%/../Packages/Debugger/Visualizers"
copy ..\doc\Lightsprint.SDK.natvis "%VS120COMNTOOLS%/../Packages/Debugger/Visualizers"
copy ..\doc\Lightsprint.SDK.natvis "%VS140COMNTOOLS%/../Packages/Debugger/Visualizers"
