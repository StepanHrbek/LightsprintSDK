@echo off

rem Builds lightmaps for Gamebryo scene Cathedral, medium quality.
rem Run Gamebryo-SceneApp-Cathedral.bat to see the same lightmaps in Gamebryo sample app.

if not exist "%GAMEBRYO_GI_PATH%Samples/Cathedral/Cathedral.gsa" (
	echo Scene not found.
	echo It is part of Emergent's Gamebryo GI Package 1.0.0 for Gamebryo 2.6.
	exit
)

BuildLightmaps.bat ^
 "%GAMEBRYO_GI_PATH%Samples/Cathedral/Cathedral.gsa" ^
 "outputpath=%GAMEBRYO_GI_PATH%Samples/Cathedral/LightMaps/" ^
 "outputext=tga" ^
 "quality=200" ^
 "emissivemultiplier=1.0" ^
 "pixelsperworldunit=0.5" ^
 "maxmapsize=1024"