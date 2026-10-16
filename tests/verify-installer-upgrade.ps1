param([Parameter(Mandatory=$true)][string]$BuildRoot,[Parameter(Mandatory=$true)][string]$Toolchain)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot
$base=(Resolve-Path -LiteralPath $BuildRoot).Path
$root=[IO.Path]::GetFullPath((Join-Path $base ('installer-upgrade-'+[Guid]::NewGuid().ToString('N'))))
if(!$root.StartsWith($base+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe fixture path'}
New-Item -ItemType Directory -Path $root | Out-Null
$parts=@('master','configurator','service','server','worker','cli','wcli')
$fixtureData=Join-Path $root 'NetLinkUpgradeFixtureData'
foreach($case in @('fresh','alpha8','alpha9','mixed')) {
    $directory=Join-Path $fixtureData $case
    New-Item -ItemType Directory -Path (Join-Path $directory 'keys'),(Join-Path $directory 'Backups') -Force | Out-Null
    # Deliberately invalid executable contents. They are never executed.
    foreach($relative in @('unknown.exe','keys/preserved-key','Backups/configuration.json')) {
        [IO.File]::WriteAllText((Join-Path $directory $relative),'INERT FIXTURE - preserve')
    }
    if($case-in@('alpha8','mixed')) {
        foreach($part in $parts){[IO.File]::WriteAllText((Join-Path $directory "veyon-$part.exe"),'INERT LEGACY FIXTURE')}
    }
    if($case-in@('alpha9','mixed')) {
        foreach($part in ($parts+@('companion'))){[IO.File]::WriteAllText((Join-Path $directory "netlink-$part.exe"),'INERT CURRENT FIXTURE')}
    }
}
& (Join-Path $Toolchain 'bin/makensis.exe') "/DREPOROOT=$repoRoot" "/DOUTPUT=$root" (Join-Path $PSScriptRoot 'InstallerUpgradeFixture.nsi') | Out-Null
if($LASTEXITCODE-ne0){throw 'Installer upgrade fixture compilation failed'}
$process=Start-Process -FilePath (Join-Path $root 'netlink-upgrade-fixture.exe') -WindowStyle Hidden -PassThru
try {
    if(!$process.WaitForExit(15000)){$process.Kill();throw 'Installer upgrade fixture timed out'}
    if($process.ExitCode-ne0){throw "Installer upgrade fixture failed ($($process.ExitCode))"}
} finally {$process.Dispose()}
foreach($case in @('fresh','alpha8','alpha9','mixed')) {
    $directory=Join-Path $fixtureData $case
    foreach($part in $parts){if(Test-Path -LiteralPath (Join-Path $directory "veyon-$part.exe")){throw "Legacy fixture not retired: $case/$part"}}
    foreach($relative in @('unknown.exe','keys/preserved-key','Backups/configuration.json')) {
        if([IO.File]::ReadAllText((Join-Path $directory $relative))-cne'INERT FIXTURE - preserve'){throw "Preserved fixture changed: $case/$relative"}
    }
    if($case-in@('alpha9','mixed')) {
        foreach($part in ($parts+@('companion'))){if([IO.File]::ReadAllText((Join-Path $directory "netlink-$part.exe"))-cne'INERT CURRENT FIXTURE'){throw "Current fixture changed: $case/$part"}}
    }
}
Write-Output 'Verified fresh/alpha.8/alpha.9/mixed upgrade selection and exact legacy retirement; keys, backups, current and unknown files preserved. No live services/registry/reboot state touched.'
