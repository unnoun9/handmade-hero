@echo off
REM this file should be run from the workspace directory
REM aka from "path-to-parent-dir-of-handmade-working-dir/handmade/"

if not exist build (
    mkdir build
)


pushd code\
clang++ -fdiagnostics-color=always -g -o ..\build\handmade.exe handmade.cpp -lgdi32 
popd