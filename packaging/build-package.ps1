param([string]$Build='build-school',[string]$Toolchain='C:/msys64/mingw64',[switch]$SkipInstaller)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot
$buildRoot=(Resolve-Path (Join-Path $repoRoot $Build)).Path
$distRoot=Join-Path $repoRoot 'dist'
$packageRoot=Join-Path $distRoot 'NetLink-0.1.0-alpha.7-win64'
if(Test-Path -LiteralPath $packageRoot){throw "Use a fresh package directory; existing files will not be overwritten: $packageRoot"}
New-Item -ItemType Directory -Path $packageRoot | Out-Null
foreach($dir in @('plugins','platforms','imageformats','crypto','translations','licenses')){New-Item -ItemType Directory -Path (Join-Path $packageRoot $dir) | Out-Null}
Get-ChildItem -LiteralPath $buildRoot -Recurse -File -Filter 'veyon-*.exe' | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $packageRoot}
Copy-Item -LiteralPath (Join-Path $buildRoot 'companion/netlink-companion.exe') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $buildRoot 'core/veyon-core.dll') -Destination $packageRoot
Get-ChildItem -LiteralPath (Join-Path $buildRoot 'plugins') -Recurse -File -Filter '*.dll' | ForEach-Object {
    $destination=if($_.Name.StartsWith('lib') -or $_.Name-eq'vnchooks.dll'){$packageRoot}else{Join-Path $packageRoot 'plugins'}
    Copy-Item -LiteralPath $_.FullName -Destination $destination
}
# Deliberately do not distribute the opaque DeskDupEngine DLL or kernel driver installer.
$qmakeTool=Join-Path $Toolchain 'bin/qmake-qt5.exe'
$qtPlugins=(& $qmakeTool -query QT_INSTALL_PLUGINS).Trim()
if($LASTEXITCODE-ne0 -or !(Test-Path -LiteralPath $qtPlugins)){throw 'Cannot locate installed Qt plugins'}
Copy-Item -LiteralPath (Join-Path $qtPlugins 'platforms/qwindows.dll') -Destination (Join-Path $packageRoot 'platforms')
Copy-Item -LiteralPath (Join-Path $qtPlugins 'imageformats/qjpeg.dll') -Destination (Join-Path $packageRoot 'imageformats')
$cryptoSource=@((Join-Path $qtPlugins 'crypto/libqca-ossl.dll'),(Join-Path $Toolchain 'share/qt5/plugins/crypto/libqca-ossl.dll'),(Join-Path $Toolchain 'lib/qt5/plugins/crypto/libqca-ossl.dll')) | Where-Object {Test-Path -LiteralPath $_} | Select-Object -First 1
if(!$cryptoSource){throw 'Missing installed QCA OpenSSL plugin'}
Copy-Item -LiteralPath $cryptoSource -Destination (Join-Path $packageRoot 'crypto')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'qt.conf') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $repoRoot 'COPYING.GPL3') -Destination (Join-Path $packageRoot 'LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $repoRoot 'COPYING') -Destination (Join-Path $packageRoot 'licenses/Veyon-GPL-2.0.txt')
Copy-Item -LiteralPath (Join-Path $repoRoot 'COPYING.GPL3') -Destination (Join-Path $packageRoot 'licenses/GPL-3.0.txt')
Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination (Join-Path $packageRoot 'README.txt')
& (Join-Path $PSScriptRoot 'verify-hook.ps1') -HookPath (Join-Path $packageRoot 'vnchooks.dll') -Toolchain $Toolchain
$translationTool=Join-Path $Toolchain 'bin/lrelease-qt5.exe'
Get-ChildItem -LiteralPath (Join-Path $repoRoot 'translations') -Filter '*.ts' | ForEach-Object {
    & $translationTool $_.FullName '-qm' (Join-Path $packageRoot ('translations/'+$_.BaseName+'.qm')) | Out-Null
    if($LASTEXITCODE-ne0){throw "Translation compilation failed: $($_.Name)"}
}
# Resolve every PE import recursively. Missing non-system imports fail packaging.
$pending=[Collections.Generic.Queue[string]]::new()
Get-ChildItem -LiteralPath $packageRoot -Recurse -File | Where-Object {$_.Extension-in@('.dll','.exe')} | ForEach-Object {$pending.Enqueue($_.FullName)}
$checked=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$systemRoot=Join-Path $env:SystemRoot 'System32'
$binaryTool=Join-Path $Toolchain 'bin/objdump.exe'
while($pending.Count){
    $binary=$pending.Dequeue();if(!$checked.Add($binary)){continue}
    $imports=& $binaryTool '-p' $binary
    if($LASTEXITCODE-ne0){throw "Cannot inspect PE imports: $binary"}
    foreach($line in $imports){if($line-match'DLL Name:\s*(\S+)'){
        $name=$Matches[1];$destination=Join-Path $packageRoot $name
        if(Test-Path -LiteralPath $destination){continue}
        if($name-match'^(api-ms-|ext-ms-)' -or (Test-Path -LiteralPath (Join-Path $systemRoot $name))){continue}
        $source=Join-Path $Toolchain "bin/$name"
        if(!(Test-Path -LiteralPath $source)){throw "Missing dependency $name imported by $binary"}
        Copy-Item -LiteralPath $source -Destination $destination;$pending.Enqueue($destination)
    }}
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs/DEPENDENCIES.md') -Destination (Join-Path $packageRoot 'licenses/DEPENDENCIES.md')
foreach($source in @((Join-Path $Toolchain 'share/licenses'))){
    # Preserve installed package license texts, never runtime binaries or developer tools.
    if(Test-Path -LiteralPath $source){Copy-Item -LiteralPath $source -Destination (Join-Path $packageRoot 'licenses/toolchain') -Recurse}
}
$manifest=Get-ChildItem -LiteralPath $packageRoot -Recurse -File | ForEach-Object {[pscustomobject]@{path=$_.FullName.Substring($packageRoot.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $distRoot 'package-manifest.json') -Encoding UTF8
$uninstallLines=$manifest | ForEach-Object {'Delete "$INSTDIR\'+$_.path.Replace('/','\').Replace('$','$$')+'"'}
$uninstallLines | Set-Content -LiteralPath (Join-Path $distRoot 'uninstall-files.nsh') -Encoding UTF8
# Build-generated manifest is not source code. Use a reproducible installer payload.
if(!$SkipInstaller){
    & (Join-Path $Toolchain 'bin/makensis.exe') "/DPAYLOAD=$packageRoot" "/DOUTPUT=$distRoot" "/DREPOROOT=$repoRoot" (Join-Path $PSScriptRoot 'veyon-school.nsi')
    if($LASTEXITCODE-ne0){throw 'Installer compilation failed'}
}
Write-Output $packageRoot
