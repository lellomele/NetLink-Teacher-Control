$ErrorActionPreference='Stop'
$root=$PSScriptRoot;$dist=Join-Path $root 'dist'
foreach($file in @('README.md','README.it.md','LICENSE.txt')){Copy-Item -LiteralPath (Join-Path $root $file) -Destination $dist}
$binaryZip=Join-Path $dist 'NetLink-Service-Check-1.2.0-win64.zip'
$sourceZip=Join-Path $dist 'NetLink-Service-Check-1.2.0-source.zip'
Compress-Archive -LiteralPath (Join-Path $dist 'NetLink-Service-Check.exe'),(Join-Path $root 'README.md'),(Join-Path $root 'README.it.md'),(Join-Path $root 'LICENSE.txt') -DestinationPath $binaryZip -Force
$stage=Join-Path $dist 'service-check-source-stage-1.2.0'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
$source=@('src','tests','branding','app.manifest','build.ps1','build-native.ps1','package.ps1','README.md','README.it.md','LICENSE.txt','.gitignore') | ForEach-Object {Join-Path $root $_}
foreach($item in $source){Copy-Item -LiteralPath $item -Destination $stage -Recurse -Force}
$native=Join-Path $stage 'native';New-Item -ItemType Directory -Force -Path (Join-Path $native 'branding') | Out-Null
$packaging=Join-Path $root '../../packaging'
if(!(Test-Path -LiteralPath (Join-Path $packaging 'StartupCheck.cpp'))){$packaging=Join-Path $root '../VeyonSchool/packaging'}
if(!(Test-Path -LiteralPath (Join-Path $packaging 'StartupCheck.cpp'))){$packaging=Join-Path $root 'native'}
foreach($file in @('StartupCheck.cpp','StartupCheckPolicy.h','Maintenance.cpp','netlink-startup-check.rc.in')){Copy-Item -LiteralPath (Join-Path $packaging $file) -Destination $native -Force}
foreach($file in @('icon.svg','netlink.ico')){$icon=Join-Path (Split-Path $packaging) ('branding/'+$file);if(!(Test-Path -LiteralPath $icon)){$icon=Join-Path $packaging ('branding/'+$file)};Copy-Item -LiteralPath $icon -Destination (Join-Path $native 'branding') -Force}
Compress-Archive -LiteralPath @(Get-ChildItem -LiteralPath $stage | ForEach-Object FullName) -DestinationPath $sourceZip -Force
$hashes=@('NetLink-Service-Check.exe','NetLink-Service-Check-1.2.0-win64.zip','NetLink-Service-Check-1.2.0-source.zip') | ForEach-Object {(Get-FileHash -LiteralPath (Join-Path $dist $_) -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+$_}
[IO.File]::WriteAllLines((Join-Path $dist 'SHA256SUMS.txt'),[string[]]$hashes,[Text.UTF8Encoding]::new($false))
Get-ChildItem -LiteralPath $dist -File | Select-Object Name,Length
