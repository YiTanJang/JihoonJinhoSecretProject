@echo off
setlocal enabledelayedexpansion
set "PROJ_ROOT=%~dp0..\.."
pushd "%PROJ_ROOT%"
set "VCVARS="
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if defined VCVARS ( call "!VCVARS!" >nul )
cl.exe /nologo /O2 /EHsc /utf-8 tools/BoardValidator.cpp /Fe:bin/BoardValidator.exe /Fo:obj/
popd
endlocal
