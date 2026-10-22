param([string]$Build='build-school',[string]$Toolchain='C:/msys64/mingw64',[string]$QtPrefix=$Toolchain,[switch]$SkipInstaller,[string]$BuildSuffix='')
$ErrorActionPreference='Stop'
if($BuildSuffix -and $BuildSuffix-notmatch'^-[a-zA-Z0-9][a-zA-Z0-9.-]*$'){throw 'Invalid diagnostic build suffix'}
$repoRoot=Split-Path $PSScriptRoot
$buildRoot=(Resolve-Path (Join-Path $repoRoot $Build)).Path
$distRoot=Join-Path $repoRoot 'dist'
$packageRoot=Join-Path $distRoot "NetLink-1.0.0-alpha.27$BuildSuffix-win64"
foreach($component in @('master','configurator','service','server','worker','cli','wcli','companion')) {
    $directory=if($component-eq'wcli'){'cli'}else{$component}
    $binary=Get-Item -LiteralPath (Join-Path $buildRoot "$directory/netlink-$component.exe")
    if($binary.VersionInfo.FileVersion-ne"4.7.5-netlink.1.0.0-alpha.27$BuildSuffix") {
        throw "Build/package version mismatch before copying: $($binary.Name)"
    }
}
if(Test-Path -LiteralPath $packageRoot){throw "Use a fresh package directory; existing files will not be overwritten: $packageRoot"}
New-Item -ItemType Directory -Path $packageRoot | Out-Null
foreach($dir in @('plugins','platforms','imageformats','crypto','translations','licenses')){New-Item -ItemType Directory -Path (Join-Path $packageRoot $dir) | Out-Null}
foreach($component in @('master','configurator','service','server','worker','cli','wcli','companion')) {
    $directory=if($component-eq'wcli'){'cli'}else{$component}
    Copy-Item -LiteralPath (Join-Path $buildRoot "$directory/netlink-$component.exe") -Destination $packageRoot
}
Copy-Item -LiteralPath (Join-Path $buildRoot 'core/veyon-core.dll') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $buildRoot 'packaging/netlink-maintenance.exe') -Destination $packageRoot
Get-ChildItem -LiteralPath (Join-Path $buildRoot 'plugins') -Recurse -File -Filter '*.dll' | ForEach-Object {
    $destination=if($_.Name.StartsWith('lib') -or $_.Name-eq'vnchooks.dll'){$packageRoot}else{Join-Path $packageRoot 'plugins'}
    Copy-Item -LiteralPath $_.FullName -Destination $destination
}
# Deliberately do not distribute the opaque DeskDupEngine DLL or kernel driver installer.
$qmakeTool=Join-Path $QtPrefix 'bin/qmake-qt5.exe'
$qtVersion=(& $qmakeTool -query QT_VERSION).Trim()
if($LASTEXITCODE-ne0 -or $qtVersion-ne'5.15.19'){throw "Packaging requires validated Qt 5.15.19, got $qtVersion"}
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
Copy-Item -LiteralPath (Join-Path $repoRoot 'README.it.md') -Destination (Join-Path $packageRoot 'README.it.md')
& (Join-Path $PSScriptRoot 'verify-hook.ps1') -HookPath (Join-Path $packageRoot 'vnchooks.dll') -Toolchain $Toolchain
$translationTool=Join-Path $QtPrefix 'bin/lrelease-qt5.exe'
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
        $source=Join-Path $QtPrefix "bin/$name"
        if(!(Test-Path -LiteralPath $source)){
            if($name-match'^Qt5.*\.dll$'){throw "Qt module missing from validated prefix: $name"}
            $source=Join-Path $Toolchain "bin/$name"
        }
        if(!(Test-Path -LiteralPath $source)){throw "Missing dependency $name imported by $binary"}
        Copy-Item -LiteralPath $source -Destination $destination;$pending.Enqueue($destination)
    }}
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs/DEPENDENCIES.md') -Destination (Join-Path $packageRoot 'licenses/DEPENDENCIES.md')
foreach($source in @((Join-Path $Toolchain 'share/licenses'))){
    # Preserve installed package license texts, never runtime binaries or developer tools.
    if(Test-Path -LiteralPath $source){Copy-Item -LiteralPath $source -Destination (Join-Path $packageRoot 'licenses/toolchain') -Recurse}
}
if($QtPrefix-ne$Toolchain){
    Copy-Item -LiteralPath (Join-Path $QtPrefix 'share/licenses/qt5-base') -Destination (Join-Path $packageRoot 'licenses/toolchain') -Recurse -Force
}
$manifest=Get-ChildItem -LiteralPath $packageRoot -Recurse -File | ForEach-Object {[pscustomobject]@{path=$_.FullName.Substring($packageRoot.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}}
$payloadHashes=$manifest | ForEach-Object {$_.sha256.ToLowerInvariant()+'  '+$_.path}
[IO.File]::WriteAllLines((Join-Path $packageRoot 'payload.sha256'),[string[]]$payloadHashes,[Text.UTF8Encoding]::new($false))
# The canonical installed manifest must match the role AND Companion selection.
$componentManifests=@{
    'payload-client.sha256'=@('netlink-master.exe')
    'payload-master-no-companion.sha256'=@('netlink-companion.exe')
    'payload-client-no-companion.sha256'=@('netlink-master.exe','netlink-companion.exe')
}
foreach($name in $componentManifests.Keys) {
    $omitted=$componentManifests[$name]
    $hashes=$manifest | Where-Object path -notin $omitted | ForEach-Object {$_.sha256.ToLowerInvariant()+'  '+$_.path}
    [IO.File]::WriteAllLines((Join-Path $packageRoot $name),[string[]]$hashes,[Text.UTF8Encoding]::new($false))
}
foreach($name in (@('payload.sha256')+@($componentManifests.Keys))) {
    $manifest+= [pscustomobject]@{path=$name;bytes=(Get-Item -LiteralPath (Join-Path $packageRoot $name)).Length;sha256=(Get-FileHash -LiteralPath (Join-Path $packageRoot $name) -Algorithm SHA256).Hash}
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $distRoot 'package-manifest.json') -Encoding UTF8
# Build-generated manifest is not source code. Use a reproducible installer payload.
if(!$SkipInstaller){
    & (Join-Path $Toolchain 'bin/makensis.exe') '/INPUTCHARSET' 'UTF8' "/DNETLINK_BUILD_SUFFIX=$BuildSuffix" "/DPAYLOAD=$packageRoot" "/DOUTPUT=$distRoot" "/DREPOROOT=$repoRoot" (Join-Path $PSScriptRoot 'veyon-school.nsi')
    if($LASTEXITCODE-ne0){throw 'Installer compilation failed'}
}
Write-Output $packageRoot
