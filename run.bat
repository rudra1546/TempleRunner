@echo off
if not exist TempleRunner.exe (
    echo TempleRunner.exe not found! Building first...
    powershell -ExecutionPolicy Bypass -File build.ps1
)
if exist TempleRunner.exe (
    echo Starting 3D Temple Runner Game...
    start TempleRunner.exe
)
