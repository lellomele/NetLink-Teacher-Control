param([string]$Toolchain='C:/msys64/mingw64',[string]$PackageCache='C:/msys64/var/cache/pacman/pkg',
    [string]$Output='build-runtime-sources',[string]$QtBasePackage='')
$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot
$outputRoot=if([IO.Path]::IsPathRooted($Output)){$Output}else{Join-Path $repoRoot $Output}
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$gitApi='repos/msys2/MINGW-packages'
function GithubJson([string]$endpoint) {
    $reply=& gh api $endpoint
    if($LASTEXITCODE-ne0){throw "GitHub API failed: $endpoint"}
    return ($reply | ConvertFrom-Json)
}
function ContentBytes([string]$path,[string]$revision) {
    $content=GithubJson "$gitApi/contents/${path}?ref=$revision"
    return ,[Convert]::FromBase64String($content.content.Replace("`n",''))
}
function ByteHash([byte[]]$bytes) {
    $algorithm=[Security.Cryptography.SHA256]::Create()
    try {return ([BitConverter]::ToString($algorithm.ComputeHash($bytes))).Replace('-','').ToLowerInvariant()}
    finally {$algorithm.Dispose()}
}
function GitBlobHash([string]$path) {
    $bytes=[IO.File]::ReadAllBytes($path)
    $header=[Text.Encoding]::UTF8.GetBytes("blob $($bytes.Length)`0")
    $algorithm=[Security.Cryptography.SHA1]::Create()
    try {return ([BitConverter]::ToString($algorithm.ComputeHash([byte[]]($header+$bytes)))).Replace('-','').ToLowerInvariant()}
    finally {$algorithm.Dispose()}
}
# Match recipes against the exact PKGBUILD hash in the installed binary's BUILDINFO,
# not against today's mutable package versions. No downloaded build script is executed.
$components=@(
    @('qt5-base','qt5-base','qtbase'),@('qca-qt5','qca','qca'),
    @('glib2','glib2','glib'),@('graphite2','graphite2','graphite2'),
    @('libiconv','libiconv','libiconv'),@('gettext-runtime','gettext','gettext'),
    @('lzo2','lzo2','lzo'),@('gcc-libs','gcc','gcc')
)
$records=@()
foreach($component in $components) {
    $package='mingw-w64-x86_64-'+$component[0]
    if($component[0]-eq'qt5-base' -and $QtBasePackage) {
        $archive=Get-Item -LiteralPath $QtBasePackage
        $packageInfo=& C:/msys64/usr/bin/tar.exe --force-local -xOf $archive.FullName .PKGINFO
        if($LASTEXITCODE-ne0){throw 'Cannot read isolated Qt package metadata'}
        $packageName=(($packageInfo | Select-String '^pkgname = ').Line -split ' = ')[1]
        if($packageName-ne$package){throw 'Wrong isolated Qt package identity'}
        $version=(($packageInfo | Select-String '^pkgver = ').Line -split ' = ')[1]
    } else {
        $packageInfo=& C:/msys64/usr/bin/pacman.exe -Q $package
        if($LASTEXITCODE-ne0){throw "Missing installed package: $package"}
        $version=($packageInfo -split '\s+')[1]
        $archive=Get-ChildItem -LiteralPath $PackageCache -File | Where-Object {$_.Name.StartsWith("$package-$version-") -and $_.Name.EndsWith('.pkg.tar.zst')} | Select-Object -First 1
    }
    if(!$archive){throw "Missing original package archive: $package $version"}
    $metadata=& C:/msys64/usr/bin/tar.exe --force-local -xOf $archive.FullName .BUILDINFO
    if($LASTEXITCODE-ne0){throw "Cannot read BUILDINFO: $package"}
    $expected=(($metadata | Select-String '^pkgbuild_sha256sum = ').Line -split ' = ')[1]
    $base='mingw-w64-'+$component[1]
    $commits=GithubJson "$gitApi/commits?path=$base/PKGBUILD&per_page=100"
    $matched=$null;$recipe=$null
    foreach($candidate in $commits) {
        $bytes=ContentBytes "$base/PKGBUILD" $candidate.sha
        if((ByteHash $bytes)-eq$expected){$matched=$candidate.sha;$recipe=[Text.Encoding]::UTF8.GetString($bytes);break}
    }
    if(!$matched){throw "No exact recipe found for $package $version ($expected)"}
    Write-Output "Matched $package $version at $matched"
    $directory=Join-Path $outputRoot $base
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $files=GithubJson "$gitApi/contents/${base}?ref=$matched"
    foreach($file in $files) {
        if($file.type-ne'file'){throw "Recipe has a subdirectory; review source collection: $($file.path)"}
        $localFile=Join-Path $directory $file.name
        if(!(Test-Path -LiteralPath $localFile)){Invoke-WebRequest -Uri $file.download_url -OutFile $localFile -TimeoutSec 120}
        if((GitBlobHash $localFile)-ne$file.sha){throw "Recipe file integrity mismatch: $($file.path)"}
    }
    if((Get-FileHash -LiteralPath (Join-Path $directory 'PKGBUILD') -Algorithm SHA256).Hash.ToLowerInvariant()-ne$expected){throw 'Recipe integrity mismatch'}
    $upstreamVersion=$version -replace '-\d+$',''
    $sourceUrl=switch($component[1]) {
        'qt5-base' {
            $commit=[regex]::Match($recipe,'(?m)^_commit=([0-9a-f]{40})').Groups[1].Value
            if(!$commit){throw 'Missing pinned Qt commit'}
            "https://invent.kde.org/qt/qt/qtbase/-/archive/$commit/qtbase-$commit.tar.gz"
        }
        'qca' {"https://download.kde.org/stable/qca/$upstreamVersion/qca-$upstreamVersion.tar.xz"}
        'glib2' {"https://download.gnome.org/sources/glib/$($upstreamVersion -replace '\.\d+$','')/glib-$upstreamVersion.tar.xz"}
        'graphite2' {"https://github.com/silnrsi/graphite/releases/download/$upstreamVersion/graphite2-$upstreamVersion.tgz"}
        'libiconv' {"https://ftp.gnu.org/pub/gnu/libiconv/libiconv-$upstreamVersion.tar.gz"}
        'gettext' {"https://ftp.gnu.org/pub/gnu/gettext/gettext-$upstreamVersion.tar.lz"}
        'lzo2' {"https://www.oberhumer.com/opensource/lzo/download/lzo-$upstreamVersion.tar.gz"}
        'gcc' {"https://ftp.gnu.org/gnu/gcc/gcc-$upstreamVersion/gcc-$upstreamVersion.tar.xz"}
    }
    $source=Join-Path $directory ([Uri]$sourceUrl).Segments[-1]
    if(!(Test-Path -LiteralPath $source)){Invoke-WebRequest -Uri $sourceUrl -OutFile $source -TimeoutSec 240}
    $sourceHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    $expectedSource=[regex]::Match($recipe,"(?s)sha256sums=\(\s*'([0-9a-f]{64}|SKIP)'").Groups[1].Value
    # makepkg's Git-source checksum is not the checksum of GitLab's generated tarball.
    # Qt is pinned by the Git commit in its exact recipe; record this archive's hash.
    if($component[1]-ne'qt5-base' -and $expectedSource -and $expectedSource-ne'SKIP' -and $expectedSource-ne$sourceHash){throw "Upstream source hash mismatch: $package"}
    # Preserve any additional upstream patches listed in the exact source array.
    $sourceArray=[regex]::Match($recipe,'(?ms)^source=\((.*?)\)').Groups[1].Value
    foreach($match in [regex]::Matches($sourceArray,'https://[^\s"''()]+\.patch')) {
        $patchUrl=$match.Value
        if($patchUrl.Contains('$')){throw "Unresolved external patch URL: $patchUrl"}
        Invoke-WebRequest -Uri $patchUrl -OutFile (Join-Path $directory ([Uri]$patchUrl).Segments[-1]) -TimeoutSec 120
    }
    $records+=[pscustomobject]@{package=$package;version=$version;recipeCommit=$matched;recipeSha256=$expected;sourceUrl=$sourceUrl;sourceSha256=$sourceHash}
    $records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'runtime-source-manifest.json') -Encoding UTF8
}
$destination=Join-Path $repoRoot 'dist/NetLink-1.0.0-alpha.24-runtime-sources.tar.gz'
& $env:SystemRoot/System32/tar.exe -czf $destination -C $outputRoot @($components | ForEach-Object {'mingw-w64-'+$_[1]}) runtime-source-manifest.json
if($LASTEXITCODE-ne0){throw 'Runtime-source archive creation failed'}
Write-Output $destination
