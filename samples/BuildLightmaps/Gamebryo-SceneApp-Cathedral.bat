@echo off

rem Runs Gamebryo sample with lightmaps built by Gamebryo-Build-Cathedral.bat

cd %GAMEBRYO_GI_PATH%\SceneApp\Win32\VC90
SceneApp.exe %GAMEBRYO_GI_PATH%\Samples\Cathedral\Cathedral.gsa %GAMEBRYO_GI_PATH%\Samples\Cathedral\LightMaps
