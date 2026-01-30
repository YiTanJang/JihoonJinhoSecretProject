@echo off
setlocal enabledelayedexpansion

:: Get the project root directory (two levels up from scripts/batch/)
set "PROJ_ROOT=%~dp0..\.."
pushd "%PROJ_ROOT%"

echo [BUILD] Checking environment in %CD%...

:: 1. Check if cl.exe is already available
where cl.exe >nul 2>nul
if %errorlevel% equ 0 (
    echo [INFO] cl.exe found in PATH.
    goto :COMPILE
)

:: 2. Search for vcvars64.bat in common locations
set "VCVARS="
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"

if defined VCVARS (
    echo [INFO] Found MSVC at: "!VCVARS!"
    call "!VCVARS!" >nul
    goto :COMPILE
)

echo [ERROR] Could not find Visual Studio or MSVC build tools.
exit /b 1

:COMPILE
echo [BUILD] Terminating old process...
taskkill /F /IM SAmultithread.exe >nul 2>nul

echo [BUILD] Compiling SAmultithread...
if not exist obj mkdir obj
if not exist bin mkdir bin

cl.exe /nologo /O2 /EHsc /utf-8 /std:c++latest /wd4819 /constexpr:steps10000000 /I include /I lib\sqlite3 src\SAmultithread.cpp src\db_manager.cpp src\globals.cpp src\logic.cpp src\mutations.cpp src\solver.cpp lib\sqlite3\sqlite3.c /Fe:bin\SAmultithread.exe /Fo:obj\

if %errorlevel% neq 0 (
    echo [BUILD] Compilation FAILED.
    popd
    exit /b %errorlevel%
)

echo [BUILD] Success! bin\SAmultithread.exe is ready.
popd
endlocal