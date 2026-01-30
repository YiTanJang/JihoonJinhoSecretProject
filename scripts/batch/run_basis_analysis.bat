@echo off
setlocal enabledelayedexpansion
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
)
cl.exe /nologo /EHsc /O2 /std:c++latest scripts\analyze_basis_digits.cpp /Fe:bin\analyze_basis_digits.exe
if %errorlevel% equ 0 bin\analyze_basis_digits.exe
