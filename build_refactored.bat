@echo off
setlocal enabledelayedexpansion

:: Search for Visual Studio 2022 vcvars64.bat
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
)

if not exist bin mkdir bin
if not exist obj mkdir obj

echo [BUILD] Compiling Refactored SAmultithread_4d...

cl.exe /nologo /O2 /EHsc /utf-8 /std:c++latest /wd4819 /constexpr:steps10000000 /I include /I lib\sqlite3 src\main.cpp src\engine\solver.cpp src\engine\mutations.cpp src\data\db_manager.cpp src\utils\globals.cpp src\core\board.cpp src\core\basis.cpp src\core\scoring.cpp lib\sqlite3\sqlite3.c /Fe:bin\SAmultithread_4d.exe /Fo"obj/"

if %errorlevel% neq 0 (
    echo [BUILD] Compilation FAILED.
    exit /b %errorlevel%
)

echo [BUILD] Success: bin\SAmultithread_4d.exe is ready.
endlocal
