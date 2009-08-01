@echo off

rem Runs Gamebryo sample with lightmaps built by Gamebryo-Build-Tutorial.bat

if not exist "%GAMEBRYO_GI_PATH%\SceneApp\Win32\VC90\SceneApp.exe" (
	echo SceneApp.exe not found.
	echo It is part of Emergent's Gamebryo GI Package 1.0.0 for Gamebryo 2.6.
	exit
)

cd %GAMEBRYO_GI_PATH%\SceneApp\Win32\VC90
SceneApp.exe %GAMEBRYO_GI_PATH%\Samples\Tutorial\Tutorial.gsa %GAMEBRYO_GI_PATH%\Samples\Tutorial\LightMaps
