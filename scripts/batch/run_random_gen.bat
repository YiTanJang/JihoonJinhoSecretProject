@echo off
setlocal enabledelayedexpansion

:: Search for vcvars64.bat
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
)

if not exist bin mkdir bin
echo [BUILD] Compiling generate_random_boards...
cl.exe /nologo /EHsc /O2 /std:c++latest scripts\generate_random_boards.cpp /Fe:bin\generate_random_boards.exe
if %errorlevel% neq 0 (
    echo [BUILD] Compilation Failed!
    exit /b %errorlevel%
)
echo [BUILD] Success. Running Generator...
bin\generate_random_boards.exe
