@echo off 

mkdir .\build
pushd .\build

cl -FAsc -Zi ..\src\main.cpp     /EHsc /link user32.lib Gdi32.lib Ws2_32.lib
set /A compile_exit_code=%errorlevel%

popd

@REM exit compile_exit_code