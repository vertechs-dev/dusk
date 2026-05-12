@echo off
:: =====================================================================
:: build.bat - Build Dusk in an x64 MSVC environment.
::
::   Usage:
::     build.bat                         (uses default preset)
::     build.bat windows-clang-debug     (override preset)
::     build.bat -- run                  (build, then launch dusk.exe)
::
::   Picks up the latest installed Visual Studio with C++ x64 tools via
::   vswhere.exe. Configure + build use the same preset name.
::
::   This script uses goto-based error handling rather than multi-line
::   `if (...)` blocks, because Visual Studio install paths contain
::   "(x86)" and parens inside cmd.exe blocks cause parsing errors.
:: =====================================================================
setlocal EnableDelayedExpansion

set "DEFAULT_PRESET=windows-msvc-relwithdebinfo"

:: --- Argument parsing ---------------------------------------------------
set "PRESET=%DEFAULT_PRESET%"
set "RUN_AFTER=0"
:parse_args
if "%~1"=="" goto parse_done
if /i "%~1"=="--" goto handle_dashdash
set "PRESET=%~1"
shift
goto parse_args
:handle_dashdash
shift
if /i "%~1"=="run" set "RUN_AFTER=1"
shift
goto parse_args
:parse_done

:: --- Locate Visual Studio via vswhere -----------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" goto no_vswhere

set "DUSK_VSDIR="
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "DUSK_VSDIR=%%i"

if not defined DUSK_VSDIR goto no_vsinstall

set "VCVARS=!DUSK_VSDIR!\VC\Auxiliary\Build\vcvars64.bat"
if not exist "!VCVARS!" goto no_vcvars

echo [build.bat] Using Visual Studio at: !DUSK_VSDIR!
echo [build.bat] Preset: !PRESET!
echo.

:: --- Activate x64 dev environment ---------------------------------------
:: vcvars64.bat is unreliable about ERRORLEVEL — it sometimes sets a non-zero
:: exit code for non-fatal warnings even when the env is configured fine. Don't
:: trust the exit code; instead verify cl.exe ended up on PATH afterward.
call "!VCVARS!"
where cl.exe >nul 2>&1
if errorlevel 1 goto vcvars_fail

:: --- Move to script directory (the repo root) ---------------------------
cd /d "%~dp0"

:: --- Configure ----------------------------------------------------------
echo === Configuring ===
cmake --preset !PRESET!
if errorlevel 1 goto configure_fail
echo.

:: --- Build --------------------------------------------------------------
echo === Building ===
cmake --build --preset !PRESET!
if errorlevel 1 goto build_fail

echo.
echo === Build succeeded ===
echo Binary: %~dp0build\!PRESET!\dusk.exe
echo.

if "!RUN_AFTER!"=="1" goto launch_game
endlocal
exit /b 0

:launch_game
echo [build.bat] Launching dusk.exe...
start "" "%~dp0build\!PRESET!\dusk.exe"
endlocal
exit /b 0

:: --- Error handlers -----------------------------------------------------
:no_vswhere
echo [build.bat] ERROR: vswhere.exe not found at:
echo     !VSWHERE!
echo Install Visual Studio with the "Desktop development with C++" workload.
goto fail

:no_vsinstall
echo [build.bat] ERROR: No Visual Studio install with C++ x64 tools found.
echo Open the Visual Studio Installer and add the
echo   "MSVC v142/v143 - VS 20xx C++ x64/x86 build tools" component.
goto fail

:no_vcvars
echo [build.bat] ERROR: vcvars64.bat not found at:
echo     !VCVARS!
goto fail

:vcvars_fail
echo [build.bat] ERROR: vcvars64.bat ran but cl.exe is not on PATH.
echo Possible causes:
echo   * The "MSVC v142/v143 - VS 20xx C++ x64/x86 build tools" component is
echo     missing from your Visual Studio install. Open the Visual Studio
echo     Installer and add it via Modify -^> Individual components.
echo   * Visual Studio install is corrupted; try Repair from the installer.
echo Detected vcvars64.bat: !VCVARS!
goto fail

:configure_fail
echo [build.bat] ERROR: Configure failed.
goto fail

:build_fail
echo [build.bat] ERROR: Build failed.
goto fail

:fail
echo.
pause
endlocal
exit /b 1
