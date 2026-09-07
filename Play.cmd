@echo off
cd /d "%~dp0"
if exist "Release\Windows\Outpost2D.exe" (
    start "" "Release\Windows\Outpost2D.exe" -windowed -ResX=1440 -ResY=960
) else (
    echo Windows build is missing. Open Outpost2D.uproject with Unreal Engine 5.8.
    pause
)
