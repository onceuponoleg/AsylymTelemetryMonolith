@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion
title ASYLYM MONOLITH // Windows Installer & Manager

set "APP_DIR=%USERPROFILE%\.asylym_monolith"
set "VENV_DIR=%APP_DIR%\venv"
set "PY_EXE=%VENV_DIR%\Scripts\python.exe"
set "PYW_EXE=%VENV_DIR%\Scripts\pythonw.exe"
set "DAEMON_DST=%APP_DIR%\telemetry_daemon.py"
set "DAEMON_SRC=%~dp0telemetry_daemon.py"
set "TASK_NAME=AsylymMonolithTelemetry"

echo ========================================================
echo   ASYLYM MONOLITH // TELEMETRY INSTALLER (WINDOWS)
echo ========================================================
echo.

if not exist "%APP_DIR%" mkdir "%APP_DIR%"

if exist "%DAEMON_SRC%" (
    copy /Y "%DAEMON_SRC%" "%DAEMON_DST%" >nul
) else if not exist "%DAEMON_DST%" (
    echo [ERROR] Please place telemetry_daemon.py in the same folder as install_win.bat.
    pause
    exit /b 1
)

:: 1. Locate or auto-install Python 3
set "SYS_PY="
where py >nul 2>&1 && (py -3 --version >nul 2>&1 && set "SYS_PY=py -3")
if "%SYS_PY%"=="" (
    where python >nul 2>&1 && (python --version >nul 2>&1 && set "SYS_PY=python")
)

if "%SYS_PY%"=="" (
    echo [1/3] Python 3 not found. Installing Python 3.12 via winget...
    where winget >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] winget is not available. Download Python 3 from https://www.python.org/downloads/
        pause
        exit /b 1
    )
    winget install -e --id Python.Python.3.12 --accept-source-agreements --accept-package-agreements
    echo.
    echo [INFO] Python installed. Please close this window and run install_win.bat again.
    pause
    exit /b 0
) else (
    echo [1/3] System Python detected: %SYS_PY%
)

:: 2. Create isolated virtual environment
if not exist "%PY_EXE%" (
    echo [2/3] Creating virtual environment in %VENV_DIR%...
    %SYS_PY% -m venv "%VENV_DIR%"
    if errorlevel 1 (
        echo [ERROR] Failed to create virtual environment.
        pause
        exit /b 1
    )
) else (
    echo [2/3] Virtual environment already exists.
)

:: 3. Install dependencies
echo [3/3] Checking and installing psutil...
"%PY_EXE%" -c "import psutil" >nul 2>&1
if errorlevel 1 (
    "%PY_EXE%" -m pip install --upgrade pip --quiet
    "%PY_EXE%" -m pip install psutil --quiet
)
echo [OK] Environment is ready.
echo.

:MENU
echo --------------------------------------------------------
echo   SELECT OPERATING MODE:
echo   [1] Run manually (interactive console window)
echo   [2] Run in DEMO mode (simulated load and alerts)
echo   [3] Install and start SILENT background service (autostart)
echo   [4] Stop and remove background service
echo   [5] Set custom ESP32 IP (leave empty for auto-discovery)
echo   [0] Exit
echo --------------------------------------------------------
set /p "CHOICE=Enter your choice: "

if "%CHOICE%"=="1" (
    echo.
    echo Starting in interactive mode. Press Ctrl+C to stop.
    "%PY_EXE%" "%DAEMON_DST%"
    goto MENU
)
if "%CHOICE%"=="2" (
    echo.
    echo Starting in DEMO mode. Press Ctrl+C to stop.
    "%PY_EXE%" "%DAEMON_DST%" --demo
    goto MENU
)
if "%CHOICE%"=="3" (
    echo.
    echo Registering silent autostart task in Windows Task Scheduler...
    schtasks /Delete /TN "%TASK_NAME%" /F >nul 2>&1
    powershell -NoProfile -Command "Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*telemetry_daemon.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }" >nul 2>&1
    schtasks /Create /TN "%TASK_NAME%" /TR "\"%PYW_EXE%\" \"%DAEMON_DST%\" --silent" /SC ONLOGON /RL LIMITED /F >nul
    start "" "%PYW_EXE%" "%DAEMON_DST%" --silent
    echo [OK] Silent service installed and started. No console window will appear.
    echo      Log file: %APP_DIR%\daemon.log
    echo.
    goto MENU
)
if "%CHOICE%"=="4" (
    echo.
    schtasks /Delete /TN "%TASK_NAME%" /F >nul 2>&1
    powershell -NoProfile -Command "Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*telemetry_daemon.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }" >nul 2>&1
    echo [OK] Background service stopped and removed from autostart.
    echo.
    goto MENU
)
if "%CHOICE%"=="5" (
    echo.
    set /p "NEW_IP=Enter ESP32 IP address (or press Enter for auto-discovery): "
    if "!NEW_IP!"=="" (
        if exist "%APP_DIR%\config.json" del "%APP_DIR%\config.json"
        echo [OK] Auto-discovery enabled (aether-monolith.local / subnet broadcast).
    ) else (
        echo {"ip": "!NEW_IP!"}> "%APP_DIR%\config.json"
        echo [OK] Custom IP saved: !NEW_IP!
    )
    echo.
    goto MENU
)
if "%CHOICE%"=="0" exit /b 0
goto MENU