@echo off

rem Builds standard and directional lightmaps and bent normals for Gamebryo scene Cathedral.
rem Run Gamebryo-SceneApp-Cathedral.bat to see the same lightmaps in Gamebryo sample app.

BuildLightmaps.bat "%GAMEBRYO_GI_PATH%Samples/Cathedral/Cathedral.gsa" "outputpath=%GAMEBRYO_GI_PATH%Samples/Cathedral/LightMaps/" "outputext=tga" "quality=200" "maxmapsize=1024" directional bentnormals