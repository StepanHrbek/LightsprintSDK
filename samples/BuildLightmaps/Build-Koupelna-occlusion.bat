@echo off

rem Build ambient occlusion in koupelna scene.
rem Saves results to data/export, runs scene viewer to display them.

BuildLightmaps.bat ../../data/scenes/koupelna/koupelna4-windows.dae "outputpath=../../data/export/koupelna" "quality=2000" occlusion viewer