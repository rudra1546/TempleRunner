# PowerShell build script for 3D Temple Runner Game
$gpp = "C:\msys64\ucrt64\bin\g++.exe"
$includes = @("-I", "C:\msys64\ucrt64\include")
$libs = @("-L", "C:\msys64\ucrt64\lib", "-lraylib", "-lopengl32", "-lgdi32", "-lwinmm")

Write-Host "Compiling main.cpp with Raylib..." -ForegroundColor Cyan
& $gpp -O2 main.cpp -o TempleRunner.exe $includes $libs

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build succeeded! Output: TempleRunner.exe" -ForegroundColor Green
} else {
    Write-Host "Build failed with exit code $LASTEXITCODE" -ForegroundColor Red
}
