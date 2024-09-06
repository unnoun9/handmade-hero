:: this file should be ran from the workspace directory
:: aka from "path-to-parent-dir-of-handmade-working-dir/handmade/"
@echo off

mkdir build
pushd code\
clang++ -fdiagnostics-color=always -g -o ..\build\handmade.exe handmade.cpp
popd