@echo off
REM ============================================================================
REM ANTLR Parser Generation Script for Windows
REM ============================================================================

echo =========================================
echo LM Compiler - Parser Generation (Windows)
echo =========================================
echo.

REM ANTLR JAR 路径 - 请根据您的实际路径修改
set ANTLR_JAR=E:\antlr\antlr-4.13.2-complete.jar

REM 检查 ANTLR JAR 是否存在
echo Checking for ANTLR4...
if exist "%ANTLR_JAR%" (
    echo [OK] Found ANTLR JAR: %ANTLR_JAR%
) else (
    echo [ERROR] ANTLR JAR not found: %ANTLR_JAR%
    echo.
    echo Please update the ANTLR_JAR path in this script
    pause
    exit /b 1
)

REM 检查 Java
echo Checking for Java...
java -version >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Java is installed
    java -version
) else (
    echo [ERROR] Java not found!
    echo Please install Java Runtime Environment (JRE) 8 or higher
    pause
    exit /b 1
)

echo.
echo Generating parser from LM.g4...
echo.

REM 进入 grammar 目录
cd /d "%~dp0..\grammar"

REM 检查语法文件
if not exist "LM.g4" (
    echo [ERROR] Grammar file not found: LM.g4
    pause
    exit /b 1
)

REM 生成解析器
java -jar "%ANTLR_JAR%" -Dlanguage=Python3 -visitor -no-listener LM.g4

if %errorlevel% equ 0 (
    echo.
    echo [OK] Parser generated successfully!
    echo.
    echo Generated files:
    dir /b LM*.py 2>nul
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
echo      python -c "from LMParser import LMParser; print('OK')"
echo.
pause
