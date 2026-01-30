@echo off
setlocal

set "PROJ_ROOT=%~dp0..\.."
pushd "%PROJ_ROOT%"

:: Search for vcvars64.bat
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
)

echo [BUILD] Compiling logic_ffi.dll (Refactored)...
if not exist bin mkdir bin
if not exist obj mkdir obj

cl.exe /nologo /O2 /LD /EHsc /utf-8 /std:c++latest /constexpr:steps10000000 /I include /I lib\sqlite3 ^
    src/ffi_wrapper.cpp ^
    src/core/board.cpp ^
    src/core/basis.cpp ^
    src/core/scoring.cpp ^
    src/utils/globals.cpp ^
    src/data/db_manager.cpp ^
    src/engine/mutations.cpp ^
    lib/sqlite3/sqlite3.c ^
    /Fe:bin/logic_ffi.dll /Fo:obj/

if %errorlevel% neq 0 (
    echo [BUILD] FAILED.
    popd
    exit /b %errorlevel%
)

echo [BUILD] Success! bin/logic_ffi.dll is ready.
popd
endlocal
