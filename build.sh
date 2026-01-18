mkdir -p build
x86_64-w64-mingw32-g++ -fdiagnostics-color=always -g -o build/handmade.exe code/win32_handmade.cpp -lgdi32 \
    &&  WINEDEBUG=-all wine build/handmade.exe
