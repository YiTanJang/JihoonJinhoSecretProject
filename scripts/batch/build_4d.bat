@echo off
setlocal enabledelayedexpansion

set "PROJ_ROOT=%~dp0..\.."
pushd "%PROJ_ROOT%"

:: Search for vcvars64.bat
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
)

if not exist obj mkdir obj
if not exist bin mkdir bin

echo [BUILD] Compiling SAmultithread_4d...
cl.exe /nologo /O2 /EHsc /utf-8 /std:c++latest /wd4819 /constexpr:steps10000000 /I include /I lib\sqlite3 src\SAmultithread_4d.cpp src\solver_4d.cpp src\db_manager_4d.cpp src\globals.cpp src\logic.cpp src\mutations.cpp lib\sqlite3\sqlite3.c /Fe:bin\SAmultithread_4d.exe /Fo:obj\

if %errorlevel% neq 0 (
    echo [BUILD] Compilation FAILED.
    popd
    exit /b %errorlevel%
)

echo [BUILD] Success bin\SAmultithread_4d.exe is ready.
popd
endlocal
