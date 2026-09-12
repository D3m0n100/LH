@echo off
REM ============================================================================
REM ANTLR Parser Generation Script for Windows
REM ============================================================================

echo =========================================
echo LH Compiler - Parser Generation (Windows)
echo =========================================
echo.

REM ANTLR JAR 路径解析 - 转换为绝对路径，并自动搜索常见候选位置
setlocal enabledelayedexpansion

set "ANTLR_JAR="
set "USE_ANTLR_CMD=0"

REM 1. 优先检查命令行参数
if not "%~1"=="" (
    for %%I in ("%~1") do (
        if exist "%%~fI" set "ANTLR_JAR=%%~fI"
    )
)

REM 2. 检查环境变量 ANTLR_JAR
if "%ANTLR_JAR%"=="" if not "%ANTLR_JAR_ENV%"=="" (
    for %%I in ("%ANTLR_JAR_ENV%") do (
        if exist "%%~fI" set "ANTLR_JAR=%%~fI"
    )
)
if "%ANTLR_JAR%"=="" if defined ANTLR_JAR (
    for %%I in ("%ANTLR_JAR%") do (
        if exist "%%~fI" set "ANTLR_JAR=%%~fI"
    )
)

REM 3. 搜索常见候选目录
if "%ANTLR_JAR%"=="" (
    for %%P in (
        "%~dp0antlr-4.13.2-complete.jar"
        "%~dp0..\antlr-4.13.2-complete.jar"
        "%~dp0..\..\tools\antlr-4.13.2-complete.jar"
        "%~dp0..\..\..\tools\antlr-4.13.2-complete.jar"
        "%~dp0..\..\..\..\tools\antlr-4.13.2-complete.jar"
        "%USERPROFILE%\antlr-4.13.2-complete.jar"
        "%CD%\antlr-4.13.2-complete.jar"
    ) do (
        if "%%~fP"=="" (
            rem skip
        ) else if exist "%%~fP" (
            if "!ANTLR_JAR!"=="" set "ANTLR_JAR=%%~fP"
        )
    )
)

REM 4. 检查系统 PATH 中是否有 antlr4 命令
if "%ANTLR_JAR%"=="" (
    where antlr4 >nul 2>&1
    if !errorlevel! equ 0 (
        set "USE_ANTLR_CMD=1"
    )
)

REM 检查是否成功找到 ANTLR
echo Checking for ANTLR4...
if not "%ANTLR_JAR%"=="" (
    echo [OK] Found ANTLR JAR: %ANTLR_JAR%
) else if "%USE_ANTLR_CMD%"=="1" (
    echo [OK] Found ANTLR4 CLI command in PATH
) else (
    echo [ERROR] ANTLR4 not found!
    echo.
    echo Please set ANTLR_JAR environment variable or pass the JAR path as first argument:
    echo   generate_parser.bat [path\to\antlr-4.13.2-complete.jar]
    echo Or install antlr4 command-line tool.
    pause
    exit /b 1
)

REM 检查 Java（若使用 JAR 则必须需要 Java）
if "%USE_ANTLR_CMD%"=="0" (
    echo Checking for Java...
    java -version >nul 2>&1
    if !errorlevel! equ 0 (
        echo [OK] Java is installed
    ) else (
        echo [ERROR] Java not found!
        echo Please install Java Runtime Environment (JRE) 8 or higher
        pause
        exit /b 1
    )
)

echo.
echo Generating parser from LH.g4...
echo.

REM 进入 grammar 目录
cd /d "%~dp0..\grammar"

REM 检查语法文件
if not exist "LH.g4" (
    echo [ERROR] Grammar file not found: LH.g4
    pause
    exit /b 1
)

REM 生成解析器
if "%USE_ANTLR_CMD%"=="1" (
    antlr4 -Dlanguage=Python3 -visitor -no-listener LH.g4
) else (
    java -jar "%ANTLR_JAR%" -Dlanguage=Python3 -visitor -no-listener LH.g4
)

if %errorlevel% equ 0 (
    echo.
    echo [OK] Parser generated successfully!
    echo.
    echo Generated files:
    dir /b LH*.py 2>nul
    dir /b *.tokens 2>nul
    echo.
) else (
    echo.
    echo [ERROR] Parser generation failed!
    pause
    exit /b 1
)

REM 创建 __init__.py
if not exist "__init__.py" (
    type nul > __init__.py
    echo [OK] Created __init__.py
)

echo.
echo =========================================
echo [OK] Parser generation complete!
echo =========================================
echo.
echo Next steps:
echo   1. Test the parser:
echo      cd grammar
echo      python -c "from LHParser import LHParser; print('OK')"
echo.
pause
