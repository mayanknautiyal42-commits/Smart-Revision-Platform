@echo off
cd /d "%~dp0"
if not exist data mkdir data
g++ -std=c++11 -o smart_revision.exe main.cpp
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)
smart_revision.exe
pause
