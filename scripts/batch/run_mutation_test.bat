@echo off
setlocal enabledelayedexpansion

:: Search for vcvars64.bat
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
)

if not exist bin mkdir bin
echo [BUILD] Compiling verify_mutations_standalone...
cl.exe /nologo /EHsc /O2 /std:c++latest tests\verify_mutations_standalone.cpp /Fe:bin\verify_mutations_standalone.exe
if %errorlevel% neq 0 (
    echo [BUILD] Compilation Failed!
    exit /b %errorlevel%
)
echo [BUILD] Success. Running Test...
bin\verify_mutations_standalone.exe
