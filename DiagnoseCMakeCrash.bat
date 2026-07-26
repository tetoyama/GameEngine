@echo off
rem ======================================================================
rem
rem  DiagnoseCMakeCrash.bat
rem
rem  Isolates why CMake crashes (exit code -1073740791 / 0xC0000409) when
rem  configuring ggml.
rem
rem  ggml declares:  project("ggml" C CXX ASM)
rem  The crash happens right after the C and CXX compiler identification
rem  and before the ASM one, with both CMake 4.0.3 and the VS-bundled 3.x,
rem  on both a OneDrive path and a plain ASCII path.
rem
rem  This script configures three MINIMAL projects that contain nothing
rem  but a project() call, and reports which of them crashes:
rem
rem    A) project(t C)            -- C only
rem    B) project(t C CXX)        -- C and C++
rem    C) project(t C CXX ASM)    -- same as ggml
rem
rem  Interpretation:
rem    A fails            -> the toolchain/environment itself is broken
rem    A,B ok  C fails    -> ASM language detection is the trigger
rem    all ok             -> the trigger is inside ggml, not the toolchain
rem
rem  Nothing is built. Each step takes a few seconds.
rem  ASCII-only on purpose (see BuildVulkanBackend.bat for why).
rem
rem ======================================================================
setlocal enabledelayedexpansion

set "DIAG_ROOT=C:\cmake-diag"

echo ======================================================
echo  CMake crash isolation
echo ======================================================
echo.

rem ------------------------------------------------------
rem Locate the tools
rem ------------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_PATH="
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%p in (`"%VSWHERE%" -latest -products * -property installationPath 2^>nul`) do (
        set "VS_PATH=%%p"
    )
)
if not defined VS_PATH (
    echo [NG] Visual Studio 2022 not found.
    goto :done
)
echo   Visual Studio : %VS_PATH%

set "CMAKE_EXE=cmake"
set "VS_CMAKE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if exist "%VS_CMAKE%" set "CMAKE_EXE=%VS_CMAKE%"
echo   cmake         : %CMAKE_EXE%

set "NINJA_EXE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
echo   ninja         : %NINJA_EXE%

set "VCVARS=%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
echo   vcvars        : %VCVARS%
echo.

rem MASM is what the ASM language needs on MSVC.
echo   Checking for the MASM assembler...
if exist "%VCVARS%" (
    call "%VCVARS%" >nul 2>&1
)
where ml64 >nul 2>&1
if errorlevel 1 (
    echo   ml64.exe      : NOT FOUND  ^<-- this alone can break project^(... ASM^)
) else (
    for /f "tokens=*" %%m in ('where ml64') do echo   ml64.exe      : %%m
)
echo.

rem ------------------------------------------------------
rem Prepare the minimal projects
rem ------------------------------------------------------
if exist "%DIAG_ROOT%" rmdir /s /q "%DIAG_ROOT%"
mkdir "%DIAG_ROOT%\a" 2>nul
mkdir "%DIAG_ROOT%\b" 2>nul
mkdir "%DIAG_ROOT%\c" 2>nul

> "%DIAG_ROOT%\a\CMakeLists.txt" echo cmake_minimum_required(VERSION 3.14)
>>"%DIAG_ROOT%\a\CMakeLists.txt" echo project(t C)

> "%DIAG_ROOT%\b\CMakeLists.txt" echo cmake_minimum_required(VERSION 3.14)
>>"%DIAG_ROOT%\b\CMakeLists.txt" echo project(t C CXX)

> "%DIAG_ROOT%\c\CMakeLists.txt" echo cmake_minimum_required(VERSION 3.14)
>>"%DIAG_ROOT%\c\CMakeLists.txt" echo project(t C CXX ASM)

rem ------------------------------------------------------
rem Run them
rem ------------------------------------------------------
call :try a "project(t C)"
call :try b "project(t C CXX)"
call :try c "project(t C CXX ASM)  [same as ggml]"

echo.
echo ======================================================
echo  Summary
echo ======================================================
echo   A  project(t C)          exit %RES_a%
echo   B  project(t C CXX)      exit %RES_b%
echo   C  project(t C CXX ASM)  exit %RES_c%
echo.
echo   exit 0            = fine
echo   exit -1073740791  = CMake crashed (0xC0000409)
echo.
echo   If A already fails, the toolchain itself is broken.
echo   If only C fails, ASM language detection is the trigger.
echo   If all pass, the trigger is inside ggml, not the toolchain.
echo.

:done
pause
exit /b 0

rem ------------------------------------------------------
rem :try <name> <label>
rem ------------------------------------------------------
:try
set "N=%~1"
echo ------------------------------------------------------
echo  Case %N%: %~2
echo ------------------------------------------------------
"%CMAKE_EXE%" -S "%DIAG_ROOT%\%N%" -B "%DIAG_ROOT%\%N%\build" -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" -DCMAKE_BUILD_TYPE=Release
rem Capture the exit code immediately. Any command after this point
rem (including set) would otherwise be the one we end up reporting.
set "E=!ERRORLEVEL!"
set "RES_%N%=!E!"
echo   -^> exit code !E!
echo.
exit /b 0
