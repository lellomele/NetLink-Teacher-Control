param([string]$Output='build-qt51519',[string]$MsysRoot='C:/msys64')
$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot
$outputRoot=[IO.Path]::GetFullPath((Join-Path $repoRoot $Output))
$qtRoot=Join-Path $outputRoot 'mingw64'
if(Test-Path -LiteralPath $qtRoot){throw "Use a fresh Qt prefix; existing files will not be overwritten: $qtRoot"}
$cache=Join-Path $outputRoot 'packages'
New-Item -ItemType Directory -Force -Path $cache | Out-Null
$packages=@(
    @{name='mingw-w64-x86_64-qt5-base-5.15.19+kde+r96-1-any.pkg.tar.zst';sha256='38b5b4dac5f706b61796b4d0833f0163e765c7243062546a379f04de2357fe2e'},
    @{name='mingw-w64-x86_64-qt5-tools-5.15.19-1-any.pkg.tar.zst';sha256='7be6ebe679944f381052bb61f37f723e0d1d1b46e050a5ccff08d1f9d0469909'}
)
$tar=Join-Path $MsysRoot 'usr/bin/tar.exe'
$tarOutput=(& (Join-Path $MsysRoot 'usr/bin/cygpath.exe') -u $outputRoot).Trim()
if($LASTEXITCODE-ne0){throw 'Cannot resolve the MSYS extraction directory'}
foreach($package in $packages) {
    $archive=Join-Path $cache $package.name
    if(!(Test-Path -LiteralPath $archive)) {
        Invoke-WebRequest -Uri ('https://mirror.msys2.org/mingw/mingw64/'+[Uri]::EscapeDataString($package.name)) -OutFile $archive -TimeoutSec 120
    }
    if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()-ne$package.sha256){throw "Qt package integrity mismatch: $($package.name)"}
    $entries=& $tar --force-local -tf $archive
    if($LASTEXITCODE-ne0){throw 'Cannot inspect Qt archive'}
    foreach($entry in $entries) {
        if($entry-match'(^/|^[A-Za-z]:|(^|/)\.\.(/|$))'){throw "Unsafe archive member: $entry"}
    }
    & $tar --force-local -xf $archive -C $tarOutput mingw64
    if($LASTEXITCODE-ne0){throw "Cannot extract Qt package: $($package.name)"}
}
# This prefix contains only Qt. The existing compiler and non-Qt runtime remain
# unchanged; callers put this bin directory before the compiler's in PATH.
$env:PATH=(Join-Path $qtRoot 'bin')+';'+(Join-Path $MsysRoot 'mingw64/bin')+';'+$env:PATH
$version=(& (Join-Path $qtRoot 'bin/qmake-qt5.exe') -query QT_VERSION).Trim()
if($LASTEXITCODE-ne0 -or $version-ne'5.15.19'){throw "Unexpected isolated Qt version: $version"}
$packages | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputRoot 'packages.json') -Encoding UTF8
Write-Output "Qt $version ready in $qtRoot"
