$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot
$output=Join-Path $repoRoot 'dist/NetLink-Prepare-Upgrade.exe'
Push-Location $PSScriptRoot
try {
    & C:/msys64/mingw64/bin/windres.exe upgrade-prepare.rc -O coff -o ../dist/upgrade-prepare-res.o
    if($LASTEXITCODE-ne0){throw 'Resource build failed'}
    & C:/msys64/mingw64/bin/g++.exe UpgradePrepare.cpp ../dist/upgrade-prepare-res.o -std=c++17 -O2 -static -municode -mwindows -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -ladvapi32 -lshell32 -lole32 -loleaut32 -luuid -o $output
    if($LASTEXITCODE-ne0){throw 'Utility build failed'}
    Write-Output $output
} finally {Pop-Location}
