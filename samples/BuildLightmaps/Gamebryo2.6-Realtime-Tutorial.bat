@echo off

rem Opens Gamebryo scene Tutorial with realtime global illumination.

if not exist "%GAMEBRYO_GI_PATH%Samples/Tutorial/Tutorial.gsa" (
	echo Scene not found.
	echo It is part of Emergent's Gamebryo GI Package 1.0.0 for Gamebryo 2.6.
	exit
)

BuildLightmaps.bat "%GAMEBRYO_GI_PATH%Samples/Tutorial/Tutorial.gsa"