# this file should be run from the workspace directory

mkdir -p build

cd code
g++ -fdiagnostics-color=always -g -o ../build/handmade.exe handmade.cpp -lgdi32 
cd ..