@echo off
rem ======================================================================
rem
rem  GenerateLlamaLib.bat
rem
rem  Regenerates llama.lib (the import library) from llama.dll.
rem
rem  Why this exists:
rem  llama.cpp release zips ship the runtime DLLs but usually NOT the
rem  import libraries. The engine links against llama.lib, so after
rem  swapping in a new llama.dll you may end up with:
rem     LNK1104: cannot open file 'llama.lib'
rem
rem  An import library is just a name table: "symbol X lives in llama.dll".
rem  It can be rebuilt from the DLL's export table, which is exactly what
rem  this script does:
rem     dumpbin /exports  ->  llama.def  ->  lib /def:
rem
rem  You usually do NOT need this if the old llama.lib is still around.
rem  C functions are not name-mangled, so an older import library keeps
rem  working with a newer DLL as long as the symbols still exist.
rem  Run this when that is no longer true.
rem
rem  ASCII-only on purpose (see BuildVulkanBackend.bat for why).
rem
rem ======================================================================
setlocal enabledelayedexpansion

set "REPO_ROOT=%~dp0"
set "DLL=%REPO_ROOT%llama.dll"
set "DEF=%REPO_ROOT%llama.def"
set "LIB=%REPO_ROOT%llama.lib"

echo ======================================================
echo  Regenerating llama.lib from llama.dll
echo ======================================================
echo.

if not exist "%DLL%" (
    echo [NG] llama.dll not found: %DLL%
    goto :fail
)
echo [OK] llama.dll

rem dumpbin and lib both live in the MSVC toolchain, so set it up first.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_PATH="
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%p in (`"%VSWHERE%" -latest -products * -property installationPath 2^>nul`) do (
        set "VS_PATH=%%p"
    )
)
if not defined VS_PATH (
    echo [NG] Visual Studio 2022 not found.
    goto :fail
)
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [NG] vcvars64.bat failed.
    goto :fail
)
echo [OK] MSVC environment

where dumpbin >nul 2>&1
if errorlevel 1 (
    echo [NG] dumpbin not found even after vcvars.
    goto :fail
)
echo [OK] dumpbin
echo.

rem Keep the previous import library so it can be put back.
if exist "%LIB%" (
    copy /Y "%LIB%" "%LIB%.bak" >nul
    echo   previous llama.lib saved as llama.lib.bak
)

rem ------------------------------------------------------
rem Build the .def file from the export table
rem ------------------------------------------------------
echo   reading exports...
> "%DEF%" echo LIBRARY llama
>>"%DEF%" echo EXPORTS

rem dumpbin's export section lists rows like:
rem     1    0 00012345 llama_backend_init
rem Take the 4th column, and only for lines whose first column is a number.
set "COUNT=0"
for /f "usebackq tokens=1,2,3,4" %%a in (`dumpbin /exports "%DLL%"`) do (
    echo %%a| findstr /r "^[0-9][0-9]*$" >nul
    if not errorlevel 1 (
        if not "%%d"=="" (
            >>"%DEF%" echo     %%d
            set /a COUNT+=1
        )
    )
)

if %COUNT%==0 (
    echo [NG] No exports were parsed from llama.dll.
    goto :fail
)
echo [OK] %COUNT% exported symbols
echo.

rem ------------------------------------------------------
rem Build the import library
rem ------------------------------------------------------
echo   creating llama.lib...
lib /nologo /def:"%DEF%" /machine:x64 /out:"%LIB%"
if errorlevel 1 (
    echo [NG] lib.exe failed.
    goto :fail
)

echo.
echo ======================================================
echo  Done
echo ======================================================
echo   %LIB%
echo   ^(llama.def was kept for reference; llama.exp is a by-product^)
echo.
echo   Rebuild the engine now.
echo   If the link still fails on a specific llama_* symbol, that symbol
echo   no longer exists in this llama.dll and the calling code has to be
echo   updated to the new API.
echo.
pause
exit /b 0

:fail
echo.
echo ======================================================
echo  FAILED
echo ======================================================
echo   If llama.lib.bak exists, restore it with:
echo     copy /Y llama.lib.bak llama.lib
echo.
pause
exit /b 1
