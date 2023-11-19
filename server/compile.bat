@echo off 

mkdir .\build
pushd .\build

cl -FAsc -Zi ..\src\main.cpp     /EHsc /link user32.lib Gdi32.lib Ws2_32.lib

popd