@echo off
REM this file should be run from the workspace directory

if not exist build (
    mkdir build
)


pushd code\
g++ -fdiagnostics-color=always -g -o ..\build\handmade.exe handmade.cpp -lgdi32 
popd