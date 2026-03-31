$cmake = "C:\Program Files\CMake\bin\cmake.exe"
$source = Resolve-Path "$PSScriptRoot\.."

& $cmake -S $source -B "$source\build\mingw-debug" `
  -G "MinGW Makefiles" `
  -DCMAKE_C_COMPILER="D:/APP/Mingw-64/Mingw-64/ucrt64/bin/gcc.exe" `
  -DCMAKE_CXX_COMPILER="D:/APP/Mingw-64/Mingw-64/ucrt64/bin/g++.exe" `
  -DCMAKE_BUILD_TYPE=Debug

