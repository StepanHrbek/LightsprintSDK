@echo off

rem Runs Gamebryo sample with lightmaps built by Gamebryo-Build-Tutorial.bat

cd %GAMEBRYO_GI_PATH%\SceneApp\Win32\VC90
SceneApp.exe %GAMEBRYO_GI_PATH%\Samples\Tutorial\Tutorial.gsa %GAMEBRYO_GI_PATH%\Samples\Tutorial\LightMaps
