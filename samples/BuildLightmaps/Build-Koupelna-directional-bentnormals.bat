@echo off

rem Builds lightmaps, directional lightmaps and bent normals in koupelna scene.
rem Uses 256x256 maps for object 0, vertex buffers for other objects.
rem Saves them to data/export and runs scene viewer to display them (lightmaps only).

BuildLightmaps.bat ../../data/scenes/koupelna/koupelna4-windows.dae directional bentnormals viewer "quality=500" "mapsize=0" object:0 "mapsize=256"