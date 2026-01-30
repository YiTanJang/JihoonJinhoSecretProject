@echo off
setlocal enabledelayedexpansion

set "PROJ_ROOT=%~dp0..\.."
pushd "%PROJ_ROOT%"

:: Environment Setup
where cl.exe >nul 2>nul
if %errorlevel% equ 0 goto :COMPILE

set "VCVARS="
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"

if defined VCVARS (
    call "!VCVARS!" >nul
    goto :COMPILE
)
echo [ERROR] MSVC not found.
exit /b 1

:COMPILE
if not exist bin mkdir bin
cl /EHsc /std:c++20 /O2 /I include tools\PermuteMyBoard.cpp /Fe:bin\PermuteMyBoard.exe

popd
endlocal