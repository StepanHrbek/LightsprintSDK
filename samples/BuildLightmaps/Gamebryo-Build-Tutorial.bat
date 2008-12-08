@echo off

rem Builds standard lightmaps for Gamebryo scene Tutorial.
rem Run Gamebryo-SceneApp-Tutorial.bat to see the same lightmaps in Gamebryo sample app.

BuildLightmaps.bat "%GAMEBRYO_GI_PATH%Samples/Tutorial/Tutorial.gsa" "outputpath=%GAMEBRYO_GI_PATH%Samples/Tutorial/LightMaps/" "outputext=tga" "quality=1000"