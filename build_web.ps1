# Build WebAssembly target using Emscripten toolchain
Write-Host "Building WebAssembly / WebGL target for FloodRunner..."

$emsdkBat = "E:\emsdk\emsdk_env.bat"
if (-not (Test-Path $emsdkBat)) {
    Write-Error "emsdk_env.bat not found at $emsdkBat"
    exit 1
}

$env:NODE_OPTIONS = "--max-old-space-size=4096"

$buildCmd = "call `"$emsdkBat`" && cd /d `"E:\c++`" && if exist build-web rmdir /s /q build-web && emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release && cmake --build build-web --config Release"
cmd /c $buildCmd

if ($LASTEXITCODE -eq 0) {
    Write-Host "Web build succeeded! Output files in build-web/"
} else {
    Write-Error "Web build failed with exit code $LASTEXITCODE"
}
