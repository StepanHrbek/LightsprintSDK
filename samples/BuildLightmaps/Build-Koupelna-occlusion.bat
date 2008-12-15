@echo off

rem Build ambient occlusion in koupelna scene.
rem Uses 256x256 map for object 0, vertex buffers for other objects.
rem Saves results to data/export, runs scene viewer to display them.

BuildLightmaps.bat ../../data/scenes/koupelna/koupelna4-windows.dae viewer occlusion "outputpath=../../data/export/koupelna" "quality=2000" "mapsize=0" object:0 "mapsize=256"