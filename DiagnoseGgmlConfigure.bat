@echo off
rem ======================================================================
rem
rem  DiagnoseGgmlConfigure.bat
rem
rem  Narrows down why CMake crashes (exit -1073740791 / 0xC0000409) while
rem  configuring ggml.
rem
rem  Already ruled out:
rem    - CMake version   (4.0.3 and the VS-bundled 3.x behave the same)
rem    - Path            (OneDrive path and C:\ggml-src behave the same)
rem    - Generator       (Visual Studio and Ninja behave the same)
rem    - Toolchain       (minimal project(t C CXX ASM) configures fine)
rem
rem  So the trigger is in ggml's own CMake code. This script:
rem    Case 1: configure ggml with NO options
rem    Case 2: configure ggml with the Vulkan options
rem  and then, if something crashed, re-runs it with --trace-source so the
rem  last executed line is visible.
rem
rem  The trace is printed to the CONSOLE on purpose. Writing it to a file
rem  with --trace-redirect lost everything, because a fail-fast crash does
rem  not flush the file buffer (the previous attempt produced 0 bytes).
rem
rem  ASCII-only on purpose (see BuildVulkanBackend.bat for why).
rem
rem ======================================================================
setlocal enabledelayedexpansion

set "GGML_SRC=C:\ggml-src"

echo ======================================================
echo  ggml configure diagnosis
echo ======================================================
echo.

if not exist "%GGML_SRC%\CMakeLists.txt" (
    echo [NG] %GGML_SRC% not found.
    echo      Run BuildVulkanBackend.bat once first; it stages the sources there.
    goto :done
)

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

set "CMAKE_EXE=cmake"
set "VS_CMAKE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if exist "%VS_CMAKE%" set "CMAKE_EXE=%VS_CMAKE%"
set "NINJA_EXE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "VCVARS=%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"

echo   cmake : %CMAKE_EXE%
echo   source: %GGML_SRC%
echo.
call "%VCVARS%" >nul

rem ------------------------------------------------------
echo ------------------------------------------------------
echo  Case 1: ggml with NO options
echo ------------------------------------------------------
if exist "C:\ggml-diag1" rmdir /s /q "C:\ggml-diag1"
"%CMAKE_EXE%" -S "%GGML_SRC%" -B "C:\ggml-diag1" -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" -DCMAKE_BUILD_TYPE=Release
set "R1=!ERRORLEVEL!"
echo.
echo   -^> Case 1 exit code: !R1!
echo.

rem ------------------------------------------------------
echo ------------------------------------------------------
echo  Case 2: ggml with the Vulkan options
echo ------------------------------------------------------
if exist "C:\ggml-diag2" rmdir /s /q "C:\ggml-diag2"
"%CMAKE_EXE%" -S "%GGML_SRC%" -B "C:\ggml-diag2" -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" -DCMAKE_BUILD_TYPE=Release -DGGML_VULKAN=ON -DBUILD_SHARED_LIBS=ON -DGGML_BACKEND_DL=ON -DCMAKE_PREFIX_PATH="%VULKAN_SDK%"
set "R2=!ERRORLEVEL!"
echo.
echo   -^> Case 2 exit code: !R2!
echo.

rem ------------------------------------------------------
rem Trace whichever case failed first
rem ------------------------------------------------------
set "TRACE_CASE="
if not "!R1!"=="0" set "TRACE_CASE=1"
if not defined TRACE_CASE if not "!R2!"=="0" set "TRACE_CASE=2"

if defined TRACE_CASE (
    echo ------------------------------------------------------
    echo  Tracing Case !TRACE_CASE! -- the LAST line below is the culprit
    echo ------------------------------------------------------
    echo   Output goes to the console so nothing is lost on the crash.
    echo.
    if exist "C:\ggml-diag3" rmdir /s /q "C:\ggml-diag3"
    if "!TRACE_CASE!"=="1" (
        "%CMAKE_EXE%" -S "%GGML_SRC%" -B "C:\ggml-diag3" -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" -DCMAKE_BUILD_TYPE=Release --trace-source=CMakeLists.txt
    ) else (
        "%CMAKE_EXE%" -S "%GGML_SRC%" -B "C:\ggml-diag3" -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" -DCMAKE_BUILD_TYPE=Release -DGGML_VULKAN=ON -DBUILD_SHARED_LIBS=ON -DGGML_BACKEND_DL=ON -DCMAKE_PREFIX_PATH="%VULKAN_SDK%" --trace-source=CMakeLists.txt
    )
    echo.
)

echo ======================================================
echo  Summary
echo ======================================================
echo   Case 1  ggml, no options      exit !R1!
echo   Case 2  ggml, Vulkan options  exit !R2!
echo.
echo   exit 0            = fine
echo   exit -1073740791  = CMake crashed
echo.
echo   If Case 1 already crashes, the Vulkan options are innocent.
echo   If only Case 2 crashes, the Vulkan path is the trigger.
echo.
echo   Please copy the LAST FEW LINES of the trace section above.
echo   Each traced line looks like:
echo     C:\ggml-src\CMakeLists.txt(42^):  include(CheckIncludeFileCXX ^)
echo.

:done
pause
exit /b 0
