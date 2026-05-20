@echo off
setlocal ENABLEDELAYEDEXPANSION

REM === 1. Switch to project root ===
cd /d "%~dp0"

REM === 2. Clear potentially conflicting Python env vars ===
set "PYTHONHOME="
set "PYTHONPATH="

REM === 3. Configure DSL venv ===
set "VENV_DIR=%CD%\third_party\custom_dsp_language\compile\venv"
set "VENV_PY=%VENV_DIR%\Scripts\python.exe"

if not exist "%VENV_PY%" (
    echo [INFO] DSL virtual environment not found:
    echo   %VENV_DIR%
    echo Create it with:
    echo   cd /d "%CD%\third_party\custom_dsp_language\compile"
    echo   python -m venv venv
    echo   "%VENV_PY%" -m pip install -r requirements.txt
    pause
    goto :EOF
)

REM Verify venv python can actually run (not only file exists).
"%VENV_PY%" --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] DSL venv is broken or points to an invalid Python path:
    echo   %VENV_PY%
    echo.
    echo Recreate it with:
    echo   rmdir /s /q "%VENV_DIR%"
    echo   cd /d "%CD%\third_party\custom_dsp_language\compile"
    echo   python -m venv venv
    echo   "%VENV_PY%" -m pip install -r requirements.txt
    pause
    goto :EOF
)

REM Tell DSLCompiler to prefer this venv python.
set "PYTHON=%VENV_PY%"
set "PATH=%VENV_DIR%\Scripts;%PATH%"

REM === 4. Launch app ===
set "APP_EXE=%CD%\build_current_mingw\bin\ServoValvePlatform.exe"

if not exist "%APP_EXE%" (
    echo [ERROR] Executable not found:
    echo   %APP_EXE%
    echo Build first to generate build_current_mingw\bin\ServoValvePlatform.exe
    pause
    goto :EOF
)

echo [INFO] Starting Servo Valve Platform...
start "" "%APP_EXE%"

endlocal
