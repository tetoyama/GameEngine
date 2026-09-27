@echo off
rem ======================================================================
rem
rem  BuildVulkanBackend.bat
rem
rem  Builds ggml-vulkan.dll so BRAIN (AgentOS) can run LLM inference on GPU.
rem
rem  llama.cpp loads compute backends as DLLs at runtime.
rem  Dropping ggml-vulkan.dll next to the exe is enough --
rem  llama_backend_init() discovers and registers it automatically.
rem  No engine code change is required.
rem
rem  Usage: double-click this file, or run it from a command prompt.
rem
rem  NOTE: this file is intentionally ASCII-only.
rem  cmd.exe reads .bat files using the system ANSI codepage (CP932 on a
rem  Japanese Windows), so a UTF-8 encoded batch with Japanese text gets
rem  garbled and the parser breaks mid-line.
rem
rem  Japanese documentation: Docs/AgentOS/05_GPU_Backend_Setup.md
rem
rem ======================================================================
setlocal enabledelayedexpansion

set "REPO_ROOT=%~dp0"
set "GGML_ORIG=%REPO_ROOT%Source\GameApplication\Backends\llama\ggml"

rem Work entirely under short ASCII paths outside OneDrive.
rem The repository lives under a path with non-ASCII characters that is
rem also cloud-synced. CMake 4.0.3 was observed to crash there right after
rem compiler identification (exit code 0xC0000409). Copying the sources to a
rem plain path removes that whole class of problem, and 1103 files is cheap.
set "GGML_SRC=C:\ggml-src"
set "BUILD_DIR=C:\ggml-vulkan-build"

echo ======================================================
echo  Building ggml-vulkan.dll
echo ======================================================
echo   original : %GGML_ORIG%
echo   staged   : %GGML_SRC%
echo   build    : %BUILD_DIR%
echo   deploy   : %REPO_ROOT%
echo.

rem ------------------------------------------------------
rem Pre-flight checks
rem ------------------------------------------------------
where cmake >nul 2>&1
if errorlevel 1 (
    echo [NG] cmake not found.
    echo      Install CMake and make sure it is on PATH.
    goto :fail
)

rem --- Pick a CMake ---
rem ggml declares cmake_minimum_required(VERSION 3.14...3.28), i.e. it is
rem developed against CMake 3.x. CMake 4.0.3 crashed on this project with
rem exit code -1073740791 (0xC0000409 = fail-fast). If the CMake on PATH is
rem 4.x, prefer the one bundled with Visual Studio, which is a 3.x.
set "CMAKE_EXE=cmake"
set "CMAKE_VER="
for /f "tokens=3" %%v in ('cmake --version 2^>nul ^| findstr /i /c:"cmake version"') do (
    if not defined CMAKE_VER set "CMAKE_VER=%%v"
)
echo [OK] cmake %CMAKE_VER% ^(on PATH^)

rem --- Locate Visual Studio ---
rem We need three things from it: the developer environment (vcvars64.bat),
rem a CMake, and Ninja. All three ship with the C++ workload.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_PATH="
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%p in (`"%VSWHERE%" -latest -products * -property installationPath 2^>nul`) do (
        set "VS_PATH=%%p"
    )
)
if not defined VS_PATH (
    echo [NG] Visual Studio 2022 was not found ^(vswhere returned nothing^).
    echo      Install VS 2022 with the "Desktop development with C++" workload.
    goto :fail
)
echo [OK] Visual Studio: %VS_PATH%

set "VCVARS=%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo [NG] vcvars64.bat not found: %VCVARS%
    echo      The C++ workload is probably not installed.
    goto :fail
)

set "VS_CMAKE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if exist "%VS_CMAKE%" set "CMAKE_EXE=%VS_CMAKE%"

set "NINJA_EXE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not exist "%NINJA_EXE%" (
    where ninja >nul 2>&1
    if errorlevel 1 (
        echo [NG] ninja.exe not found.
        echo      Expected: %NINJA_EXE%
        echo      It ships with the VS C++ workload's CMake component.
        goto :fail
    )
    for /f "tokens=*" %%n in ('where ninja') do set "NINJA_EXE=%%n"
)
echo [OK] ninja: %NINJA_EXE%

rem --- Why Ninja and not the Visual Studio generator ---
rem With -G "Visual Studio 17 2022", configure crashed reproducibly
rem (exit code -1073740791 = 0xC0000409, a fail-fast abort) right after
rem the C/CXX compiler identification and before the ASM one.
rem ggml declares project("ggml" C CXX ASM), and the crash was identical
rem with two different CMake binaries (4.0.3 and the VS-bundled 3.x) and
rem with the sources on a plain ASCII path -- so it was not the CMake
rem version and not the path. Ninja avoids that generator entirely.

if "%VULKAN_SDK%"=="" (
    echo [NG] VULKAN_SDK environment variable is not set.
    echo      Install the LunarG Vulkan SDK ^(Core only is enough^).
    echo      You must open a NEW command prompt after installing.
    goto :fail
)
echo [OK] VULKAN_SDK = %VULKAN_SDK%

if not exist "%VULKAN_SDK%\Bin\glslc.exe" (
    echo [NG] glslc.exe not found at %VULKAN_SDK%\Bin\glslc.exe
    echo      The Vulkan SDK installation looks incomplete.
    goto :fail
)
echo [OK] glslc.exe

if not exist "%GGML_ORIG%\CMakeLists.txt" (
    echo [NG] ggml source not found: %GGML_ORIG%
    goto :fail
)
echo [OK] ggml source
echo.

rem ------------------------------------------------------
rem 1/4 Stage the source on a plain ASCII path
rem ------------------------------------------------------
echo ------------------------------------------------------
echo  [1/4] Staging sources to %GGML_SRC%
echo ------------------------------------------------------
rem robocopy skips files that are already up to date, so re-runs are fast.
robocopy "%GGML_ORIG%" "%GGML_SRC%" /E /NFL /NDL /NJH /NJS /NP >nul
rem robocopy returns 0-7 on success, 8 or higher on failure.
if %ERRORLEVEL% GEQ 8 (
    echo [NG] Failed to copy the sources.
    goto :fail
)
echo [OK] sources staged
echo.

rem ------------------------------------------------------
rem 2/4 Configure
rem ------------------------------------------------------
echo ------------------------------------------------------
echo  [2/4] CMake configure
echo ------------------------------------------------------

rem Start from a clean state. A half-finished configure leaves a build
rem directory with no CMakeCache.txt, which then fails later with the
rem confusing message "Error: could not load cache".
if exist "%BUILD_DIR%" (
    echo   removing previous build directory...
    rmdir /s /q "%BUILD_DIR%"
)

rem GGML_BACKEND_DL=ON is the important one.
rem Without it Vulkan gets linked into ggml.dll itself, which would mean
rem replacing the existing DLLs. With it, we get a standalone DLL to drop in.
rem
rem Output is shown live on purpose. Redirecting it to a file makes the
rem console look frozen during a step that legitimately takes minutes,
rem which invites interrupting a run that was actually fine.
rem Ninja needs cl.exe / link.exe on PATH, which vcvars64.bat sets up.
echo   setting up the MSVC environment...
call "%VCVARS%" >nul
if errorlevel 1 (
    echo [NG] vcvars64.bat failed.
    goto :fail
)

echo   This can take a few MINUTES with no output at times.
echo   Please wait and do not close this window.
echo.
"%CMAKE_EXE%" -S "%GGML_SRC%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" -DCMAKE_BUILD_TYPE=Release -DGGML_VULKAN=ON -DBUILD_SHARED_LIBS=ON -DGGML_BACKEND_DL=ON -DCMAKE_PREFIX_PATH="%VULKAN_SDK%"

set "CONFIG_EXIT=%ERRORLEVEL%"

rem Do not trust the exit code alone. Verify the artifact the next step
rem actually needs: CMakeCache.txt.
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo.
    echo [NG] Configure did not produce CMakeCache.txt ^(exit code %CONFIG_EXIT%^).
    echo.
    if "%CONFIG_EXIT%"=="-1073740791" (
        echo      Exit code -1073740791 means CMake itself CRASHED.
        echo      This is not a configuration mistake.
        echo      It already crashed the same way with the Visual Studio
        echo      generator, so the generator is not the cause either.
        echo      Next things worth checking:
        echo        - Temporarily disable real-time antivirus scanning
        echo        - Repair the Visual Studio installation
        echo        - Check Windows Event Viewer ^(Application^) for the
        echo          faulting module name of cmake.exe
    ) else (
        echo      Common causes:
        echo        - The run was interrupted before it finished
        echo        - Visual Studio 2022 with the C++ workload is not installed
        echo        - SPIRV-Headers could not be found
    )
    echo.
    echo      CMake writes details to:
    echo        %BUILD_DIR%\CMakeFiles\CMakeConfigureLog.yaml
    goto :fail
)

if not %CONFIG_EXIT%==0 (
    echo.
    echo [NG] Configure reported an error ^(exit code %CONFIG_EXIT%^).
    goto :fail
)

echo.
echo [OK] configure done ^(CMakeCache.txt created^)
echo.

rem ------------------------------------------------------
rem 3/4 Build
rem ------------------------------------------------------
echo ------------------------------------------------------
echo  [3/4] Building ^(this takes several minutes^)
echo ------------------------------------------------------
rem Ninja is a single-config generator, so the build type came from
rem CMAKE_BUILD_TYPE at configure time. --config would be ignored here.
"%CMAKE_EXE%" --build "%BUILD_DIR%" --target ggml-vulkan

if errorlevel 1 (
    echo.
    echo [NG] Build failed. Read the error above.
    goto :fail
)
echo.
echo [OK] build done
echo.

rem ------------------------------------------------------
rem 4/4 Deploy
rem ------------------------------------------------------
echo ------------------------------------------------------
echo  [4/4] Deploying the DLL
echo ------------------------------------------------------
rem The output directory depends on the CMake generator, so search for it
rem instead of hardcoding a path.
set "FOUND_DLL="
for /r "%BUILD_DIR%" %%f in (ggml-vulkan.dll) do (
    if not defined FOUND_DLL set "FOUND_DLL=%%f"
)

if not defined FOUND_DLL (
    echo [NG] ggml-vulkan.dll was not produced.
    echo      If the build succeeded but no DLL exists,
    echo      GGML_BACKEND_DL=ON probably did not take effect.
    goto :fail
)

echo   found: %FOUND_DLL%
copy /Y "%FOUND_DLL%" "%REPO_ROOT%" >nul
if errorlevel 1 (
    echo [NG] Copy failed.
    goto :fail
)

echo.
echo ======================================================
echo  Done
echo ======================================================
echo   Deployed: %REPO_ROOT%ggml-vulkan.dll
echo.
echo   How to verify:
echo     1. Start the engine
echo     2. Open the Status tab in the BRAIN panel
echo     3. It should show "GPU offload: supported"
echo     4. Tick "Offload to GPU" to reload the model on GPU
echo.
pause
exit /b 0

:fail
echo.
echo ======================================================
echo  FAILED
echo ======================================================
echo   See "Troubleshooting" in Docs/AgentOS/05_GPU_Backend_Setup.md
echo.
pause
exit /b 1
