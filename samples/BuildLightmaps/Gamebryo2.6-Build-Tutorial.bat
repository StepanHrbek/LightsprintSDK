@echo off

rem Builds standard lightmaps for Gamebryo scene Tutorial, medium quality.
rem Run Gamebryo-SceneApp-Tutorial.bat to see the same lightmaps in Gamebryo sample app.

if not exist "%GAMEBRYO_GI_PATH%Samples/Tutorial/Tutorial.gsa" (
	echo Scene not found.
	echo It is part of Emergent's Gamebryo GI Package 1.0.0 for Gamebryo 2.6.
	exit
)

BuildLightmaps.bat ^
 "%GAMEBRYO_GI_PATH%Samples/Tutorial/Tutorial.gsa" ^
 "outputpath=%GAMEBRYO_GI_PATH%Samples/Tutorial/LightMaps/" ^
 "outputext=tga" ^
 "quality=1000"
