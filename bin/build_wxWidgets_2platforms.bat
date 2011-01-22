@echo off
echo Going to build 32bit and then 64bit (because building them in parallel does not work)...
set PATH_BACKUP=%PATH%
call build_wxWidgets_platform %1 32 %2
set PATH=%PATH_BACKUP%
%~dp0build_wxWidgets_platform %1 64 %3
