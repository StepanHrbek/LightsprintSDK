@echo off
set PATH_BACKUP=%PATH%
call build_wxWidgets_platform %1 32 %2
set PATH=%PATH_BACKUP%
%~dp0build_wxWidgets_platform %1 64 %3
