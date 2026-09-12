@echo off
setlocal

cd /d "%~dp0\.."
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0workflow_dev.ps1" %*
exit /b %ERRORLEVEL%
