param([string]$Package='dist/NetLink-1.0.0-alpha.31-win64',[string]$Manifest='dist/package-manifest.json',[string]$BuildSuffix='')
$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot
$packageRoot=(Resolve-Path -LiteralPath (Join-Path $repoRoot $Package)).Path
$manifestPath=(Resolve-Path -LiteralPath (Join-Path $repoRoot $Manifest)).Path
$prefix=$packageRoot+[IO.Path]::DirectorySeparatorChar
$expected=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$entries=@(Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json)
foreach($entry in $entries) {
    $relative=[string]$entry.path
    if([IO.Path]::IsPathRooted($relative) -or ($relative -split '[/\\]') -contains '..'){throw 'Unsafe manifest path'}
    $file=[IO.Path]::GetFullPath((Join-Path $packageRoot $relative))
    if(!$file.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or !$expected.Add($file)){throw 'Invalid or duplicate manifest path'}
    $info=Get-Item -LiteralPath $file
    if($info.Length -ne $entry.bytes -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.sha256){throw "Payload integrity failed: $relative"}
}
$files=@(Get-ChildItem -LiteralPath $packageRoot -Recurse -File)
if($files.Count -ne $expected.Count){throw 'Unexpected payload file count'}
foreach($file in $files) {
    if(!$expected.Contains($file.FullName)){throw "Unlisted payload file: $($file.Name)"}
    if($file.Extension -eq '.sys' -or $file.Name -match '^(Qt[56]Test\.dll|.*tests.*\.exe|.*interception.*|DeskDupEngine.*)$'){throw "Forbidden runtime artifact: $($file.Name)"}
    if($file.Name -match '^veyon-.*\.exe$'){throw "Legacy executable in renamed payload: $($file.Name)"}
}
foreach($name in @('netlink-master.exe','netlink-configurator.exe','netlink-service.exe','netlink-server.exe','netlink-worker.exe','netlink-cli.exe','netlink-wcli.exe','netlink-companion.exe','netlink-maintenance.exe')) {
    $file=Get-Item -LiteralPath (Join-Path $packageRoot $name)
    if($file.VersionInfo.FileVersion -ne "4.7.5-netlink.1.0.0-alpha.31$BuildSuffix" -or $file.VersionInfo.OriginalFilename -ne $name){throw "Wrong application version/identity: $name"}
}
$fullHashes=@(Get-Content -LiteralPath (Join-Path $packageRoot 'payload.sha256'))
foreach($name in @('Qt5Core.dll','Qt5Gui.dll','Qt5Widgets.dll','Qt5Network.dll','platforms/qwindows.dll','imageformats/qjpeg.dll')) {
    $qtBinary=Get-Item -LiteralPath (Join-Path $packageRoot $name)
    if($qtBinary.VersionInfo.ProductVersion-ne'5.15.19.0'){throw "Mixed or unexpected Qt runtime version: $name"}
}
foreach($qtBinary in @($files | Where-Object Name -match '^Qt5.*\.dll$')) {
    if($qtBinary.VersionInfo.ProductVersion-ne'5.15.19.0'){throw "Mixed or unexpected Qt runtime version: $($qtBinary.Name)"}
}
if(@($fullHashes | Where-Object {$_ -match '  netlink-master\.exe$'}).Count-ne1){throw 'Master missing/duplicated in full manifest'}
if(@($fullHashes | Where-Object {$_ -match '  netlink-companion\.exe$'}).Count-ne1){throw 'Companion missing/duplicated in full manifest'}
$componentManifests=@{
    'payload-client.sha256'='  netlink-master\.exe$'
    'payload-master-no-companion.sha256'='  netlink-companion\.exe$'
    'payload-client-no-companion.sha256'='  netlink-(master|companion)\.exe$'
}
foreach($name in $componentManifests.Keys) {
    $actual=@(Get-Content -LiteralPath (Join-Path $packageRoot $name))
    $expectedHashes=@($fullHashes | Where-Object {$_ -notmatch $componentManifests[$name]})
    if(($actual -join "`n")-cne($expectedHashes -join "`n")){throw "Incorrect component manifest: $name"}
}
Write-Output "Verified $($files.Count) files, SHA-256 manifest, application versions and runtime exclusions."
