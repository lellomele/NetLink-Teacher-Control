param([string]$Revision='HEAD')
$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot
$distRoot=Join-Path $repoRoot 'dist'
$stage=Join-Path $distRoot 'source-stage'
if(Test-Path -LiteralPath $stage){throw 'Source staging directory already exists; use a fresh directory'}
New-Item -ItemType Directory -Path $stage | Out-Null
$prefix='VeyonSchool-0.1.0-alpha.1-source'
$gitOptions=@('-c',"core.excludesFile=$repoRoot/.gitignore",'-C',$repoRoot)
$revisionId=& git @gitOptions rev-parse --verify "$Revision^{commit}"
if($LASTEXITCODE-ne0){throw 'Invalid source revision'}
$rootArchive=Join-Path $stage 'root.tar'
& git @gitOptions archive --format=tar "--prefix=$prefix/" "--output=$rootArchive" $revisionId -- . ':(exclude)*.exe' ':(exclude)*.dll' ':(exclude)*.sys'
if($LASTEXITCODE-ne0){throw 'Root archive failed'}
& "$env:SystemRoot/System32/tar.exe" -xf $rootArchive -C $stage
if($LASTEXITCODE-ne0){throw 'Root extraction failed'}
function ArchiveSubmodules([string]$directory,[string]$revision,[string]$parentPath) {
    # Read gitlinks from the committed tree, not dirty worktree state or shell helpers.
    $tree=& git -c "core.excludesFile=$repoRoot/.gitignore" -C $directory ls-tree -r $revision
    if($LASTEXITCODE-ne0){throw 'Submodule tree inventory failed'}
    foreach($line in $tree) {
        if($line-notmatch'^160000 commit ([a-f0-9]{40})\t(.+)$'){continue}
        $sha=$Matches[1];$relative=$Matches[2]
        $path=if($parentPath){"$parentPath/$relative"}else{$relative}
        if($path.StartsWith('/') -or ($path -split '/')-contains'..'){throw 'Unsafe submodule path'}
        $moduleRoot=Join-Path $directory $relative
        $moduleArchive=Join-Path $stage ($path.Replace('/','_')+'.tar')
        & git -c "core.excludesFile=$repoRoot/.gitignore" -C $moduleRoot archive --format=tar "--prefix=$prefix/$path/" "--output=$moduleArchive" $sha -- . ':(exclude)*.exe' ':(exclude)*.dll' ':(exclude)*.sys'
        if($LASTEXITCODE-ne0){throw "Submodule archive failed: $path"}
        & "$env:SystemRoot/System32/tar.exe" -xf $moduleArchive -C $stage
        if($LASTEXITCODE-ne0){throw "Submodule extraction failed: $path"}
        ArchiveSubmodules $moduleRoot $sha $path
    }
}
ArchiveSubmodules $repoRoot $revisionId ''
$destination=Join-Path $distRoot 'VeyonSchool-0.1.0-alpha.1-source.tar.gz'
& "$env:SystemRoot/System32/tar.exe" -czf $destination -C $stage $prefix
if($LASTEXITCODE-ne0){throw 'Source compression failed'}
Write-Output $destination
