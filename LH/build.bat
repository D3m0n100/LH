@echo off
echo ========================================
echo   Servo Valve Platform - Build Script
echo ========================================
echo.

REM Change to script directory
cd /d "%~dp0"

REM Create build directory if not exists
set "BUILD_DIR=build_current_mingw"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo [1/3] Configure CMake...
cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/mingw81_64 ..
if errorlevel 1 goto :cmake_error

echo [2/3] Build project...
cmake --build . -j4
if errorlevel 1 goto :build_error

echo [3/3] Done.
echo.
echo Executable should be at: %BUILD_DIR%\bin\ServoValvePlatform.exe
echo.
pause
exit /b 0

:cmake_error
echo.
echo ERROR: CMake configure failed.
echo.
pause
exit /b 1

:build_error
echo.
echo ERROR: Build failed.
echo.
pause
exit /b 1
