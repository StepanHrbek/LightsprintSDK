@echo off

rem Builds lightmaps, directional lightmaps and bent normals in koupelna scene.
rem Saves them to data/export and runs scene viewer to display them (lightmaps only).

BuildLightmaps.bat ../../data/scenes/koupelna/koupelna4-windows.dae "outputpath=../../data/export/koupelna" "quality=500" directional bentnormals viewer