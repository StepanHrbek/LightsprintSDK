@echo off

rem Builds lightmaps for Gamebryo scene Cathedral.
rem Run Gamebryo-SceneApp-Cathedral.bat to see the same lightmaps in Gamebryo sample app.

BuildLightmaps.bat ^
 "%GAMEBRYO_GI_PATH%Samples/Cathedral/Cathedral.gsa" ^
 "outputpath=%GAMEBRYO_GI_PATH%Samples/Cathedral/LightMaps/" ^
 "outputext=tga" ^
 "quality=200" ^
 "emissivemultiplier=3" ^
 "maxmapsize=1024"