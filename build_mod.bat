REM rmdir /Q /S build
cmake -G "Visual Studio 18 2026" -A x64 --preset Release -B ./build
cmake --build ./build --config Release --target ALL_BUILD