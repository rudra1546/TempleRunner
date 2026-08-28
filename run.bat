@echo off
powershell -ExecutionPolicy Bypass -File build.ps1
if exist TempleRunner.exe (
    echo Starting 3D Temple Runner Game...
    start TempleRunner.exe
)
