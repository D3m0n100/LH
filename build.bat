@echo off
setlocal
echo ========================================
echo   Servo Valve Platform - Build Script
echo ========================================
echo.

set "PATH=C:\Qt\5.15.2\mingw81_64\bin;C:\Qt\Tools\mingw810_64\bin;%PATH%"

REM Change to script directory
cd /d "%~dp0"

REM Create build directory if not exists
set "BUILD_DIR=%~dp0build_current_mingw"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo [1/4] Configure CMake...
cmake -S "%~dp0." -B "%BUILD_DIR%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/mingw81_64 -DBUILD_TESTING=ON -DLH_ENABLE_TEST_FAILURE_INJECTION=OFF
if errorlevel 1 goto :cmake_error

echo [2/4] Build project...
cmake --build . -j4
if errorlevel 1 goto :build_error

echo [3/4] Run tests...
ctest --output-on-failure --timeout 120
if errorlevel 1 goto :test_error

echo [4/4] Done.
echo.
echo Executable should be at: %BUILD_DIR%\bin\LH.exe
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

:test_error
echo.
echo ERROR: Tests failed.
echo.
pause
exit /b 1
