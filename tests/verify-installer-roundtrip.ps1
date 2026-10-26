param([Parameter(Mandatory=$true)][string]$BuildRoot,[Parameter(Mandatory=$true)][string]$Toolchain,[Parameter(Mandatory=$true)][string]$Helper,[Parameter(Mandatory=$true)][string]$Cli)
$ErrorActionPreference='Stop'
# The actual installer launches netlink-wcli (Windows GUI subsystem), not a
# console program vulnerable to an unrelated console-close/Ctrl-C notification.
$cliReader=[IO.BinaryReader]::new([IO.File]::OpenRead($Cli))
try {
    $stream=$cliReader.BaseStream
    if($stream.Length-lt64){throw 'Invalid CLI fixture PE header'}
    [void]$stream.Seek(0x3c,[IO.SeekOrigin]::Begin);$peOffset=$cliReader.ReadInt32()
    if($peOffset-lt64 -or $peOffset+94-gt$stream.Length){throw 'Invalid CLI fixture PE offset'}
    [void]$stream.Seek($peOffset,[IO.SeekOrigin]::Begin)
    if($cliReader.ReadUInt32()-ne0x4550){throw 'Invalid CLI fixture PE signature'}
    [void]$stream.Seek($peOffset+92,[IO.SeekOrigin]::Begin)
    if($cliReader.ReadUInt16()-ne2){throw 'CLI fixture must match netlink-wcli: Windows GUI subsystem required'}
} finally {$cliReader.Dispose()}
$repoRoot=Split-Path $PSScriptRoot
$base=(Resolve-Path -LiteralPath $BuildRoot).Path
$id=[Guid]::NewGuid().ToString('N')
$root=[IO.Path]::GetFullPath((Join-Path $base ('installer-roundtrip-'+$id)))
if(!$root.StartsWith($base+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe installer fixture root'}
$app=Join-Path $root 'NetLinkFixtureApp'
$payload=Join-Path $root 'payload'
New-Item -ItemType Directory -Path (Join-Path $payload 'platforms'),$app,(Join-Path $root 'keys') | Out-Null
[IO.File]::WriteAllText((Join-Path $root '.netlink-install-fixture'),'ISOLATED FIXTURE ONLY')
[IO.File]::WriteAllText((Join-Path $app 'netlink-wcli.exe'),'INERT OLD CLI - MUST NOT EXECUTE')
[IO.File]::WriteAllText((Join-Path $app 'unknown.txt'),'REMOVE WITH APPLICATION DIRECTORY')
[IO.File]::WriteAllText((Join-Path $root 'keys/netlink_test'),'INERT KEY - MUST SURVIVE')
$keyHash=(Get-FileHash -LiteralPath (Join-Path $root 'keys/netlink_test')).Hash
$keyAcl=(Get-Acl -LiteralPath (Join-Path $root 'keys/netlink_test')).Sddl
Copy-Item -LiteralPath $Helper -Destination (Join-Path $payload 'netlink-maintenance.exe')
Copy-Item -LiteralPath $Cli -Destination (Join-Path $payload 'netlink-wcli.exe')
foreach($component in @('master','configurator','companion')) {
    Copy-Item -LiteralPath $Cli -Destination (Join-Path $payload "netlink-$component.exe")
}
[IO.File]::WriteAllText((Join-Path $payload 'platforms/qwindows.dll'),'INERT PLATFORM FIXTURE')
[IO.File]::WriteAllText((Join-Path $payload 'vnchooks.dll'),'NEW HOOK FIXTURE - MUST NOT OVERWRITE A LOADED IMAGE')
Copy-Item -LiteralPath (Join-Path $env:SystemRoot 'System32/version.dll') -Destination (Join-Path $app 'vnchooks.dll')
$hashes=@(Get-ChildItem -LiteralPath $payload -Recurse -File | ForEach-Object {(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()+'  '+$_.FullName.Substring($payload.Length+1).Replace('\','/')})
[IO.File]::WriteAllLines((Join-Path $payload 'payload.sha256'),[string[]]$hashes,[Text.UTF8Encoding]::new($false))
$clientHashes=@($hashes | Where-Object {$_ -notmatch '  netlink-master\.exe$'})
[IO.File]::WriteAllLines((Join-Path $payload 'payload-client.sha256'),[string[]]$clientHashes,[Text.UTF8Encoding]::new($false))
$masterMinimalHashes=@($hashes | Where-Object {$_ -notmatch '  netlink-companion\.exe$'})
$clientMinimalHashes=@($hashes | Where-Object {$_ -notmatch '  netlink-(master|companion)\.exe$'})
[IO.File]::WriteAllLines((Join-Path $payload 'payload-master-no-companion.sha256'),[string[]]$masterMinimalHashes,[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllLines((Join-Path $payload 'payload-client-no-companion.sha256'),[string[]]$clientMinimalHashes,[Text.UTF8Encoding]::new($false))
$registry='Software\NetLinkInstallerTests\'+(Split-Path $root -Leaf)
$settings=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($registry+'\Configuration\Authentication')
$settings.SetValue('PrivateKeyBaseDir',(Join-Path $root 'keys'))
$settings.SetValue('PublicKeyBaseDir',(Join-Path $root 'keys'))
$settings.SetValue('OtherSetting','PRESERVE ON UPGRADE - REMOVE ON PURGE')
$settings.Dispose()
$script:successfulInstalls=0
function RunInstaller([string]$file,[string]$arguments,[int]$expectedExit=0) {
    $errorFile=Join-Path $root 'fixture-error.txt'
    if(Test-Path -LiteralPath $errorFile){Remove-Item -LiteralPath $errorFile}
    $process=Start-Process -FilePath $file -ArgumentList $arguments -WindowStyle Hidden -PassThru
    try {if(!$process.WaitForExit(60000)){$process.Kill();throw "Installer fixture deadline exceeded: $file $arguments"}; if($process.ExitCode-ne$expectedExit){$detail=if(Test-Path -LiteralPath $errorFile){Get-Content -LiteralPath $errorFile -Raw}else{'no helper error'}; $trace=Join-Path $root 'installer-cli-trace.txt'; if(Test-Path -LiteralPath $trace){$detail+="`n"+((Get-Content -LiteralPath $trace | Select-Object -Last 12)-join"`n")}; throw "Installer fixture failed after $script:successfulInstalls successful installs: exit $($process.ExitCode), expected $expectedExit; command '$file $arguments'; $detail"}}finally{$process.Dispose()}
    if($expectedExit-eq0 -and (Split-Path $file -Leaf)-eq'NetLink-1.0.0-alpha.31-win64-setup.exe'){$script:successfulInstalls++}
}
function WaitRemoval {
    $deadline=[DateTime]::UtcNow.AddSeconds(20)
    do {
        $entry=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registry+'\Uninstall')
        $registered=$null-ne$entry
        if($entry){$entry.Dispose()}
        if(!(Test-Path -LiteralPath $app) -and !$registered){return}
        Start-Sleep -Milliseconds 100
    }while([DateTime]::UtcNow-lt$deadline)
    throw 'Uninstaller left application folder/files or its registry entry'
}
function CheckComponents([string]$role,[bool]$companion=$false) {
    $isMaster=$role-eq'Master'
    foreach($relative in @('netlink-master.exe','StartMenu/NetLink Console docente.lnk')) {
        if((Test-Path -LiteralPath (Join-Path $app $relative))-ne$isMaster){throw "Wrong $role payload/shortcut: $relative"}
    }
    foreach($relative in @('netlink-configurator.exe','StartMenu/NetLink Configurator.lnk')) {
        if(!(Test-Path -LiteralPath (Join-Path $app $relative))){throw "Missing $role component: $relative"}
    }
    foreach($relative in @('netlink-companion.exe','StartMenu/NetLink Companion.lnk')) {
        if((Test-Path -LiteralPath (Join-Path $app $relative))-ne$companion){throw "Wrong Companion selection: $relative"}
    }
    foreach($label in @('Console docente','Configurator','Companion')) {
        if(Test-Path -LiteralPath (Join-Path $app ('StartMenu/'+$label+'.lnk'))){throw "Unprefixed legacy shortcut remains: $label"}
    }
    $shell=New-Object -ComObject WScript.Shell
    try {
        $links=@{'NetLink Configurator'='netlink-configurator.exe'}
        if($companion){$links['NetLink Companion']='netlink-companion.exe'}
        if($isMaster){$links['NetLink Console docente']='netlink-master.exe'}
        foreach($label in $links.Keys) {
            $shortcut=$shell.CreateShortcut((Join-Path $app ('StartMenu/'+$label+'.lnk')))
            try {
                if($shortcut.TargetPath-ne(Join-Path $app $links[$label])){throw "Wrong shortcut target: $label"}
            } finally {[void][Runtime.InteropServices.Marshal]::ReleaseComObject($shortcut)}
        }
    } finally {[void][Runtime.InteropServices.Marshal]::ReleaseComObject($shell)}
    foreach($name in @('payload-client.sha256','payload-master-no-companion.sha256','payload-client-no-companion.sha256')) {
        if(Test-Path -LiteralPath (Join-Path $app $name)){throw "Packaging-only manifest was installed: $name"}
    }
    $entry=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registry+'\Uninstall')
    try {
        if(!$entry -or $entry.GetValue('InstallationRole')-ne$role){throw 'Installed role not persisted'}
        if($entry.GetValue('InstallCompanion')-ne[int]$companion){throw 'Companion selection not persisted'}
        if($entry.GetValue('DisplayIcon')-ne(Join-Path $app 'netlink-configurator.exe')){throw 'Uninstall icon refers to optional Master'}
    } finally {if($entry){$entry.Dispose()}}
    $expectedManifest=if($isMaster){if($companion){'payload.sha256'}else{'payload-master-no-companion.sha256'}}else{if($companion){'payload-client.sha256'}else{'payload-client-no-companion.sha256'}}
    if((Get-FileHash -LiteralPath (Join-Path $app 'payload.sha256')).Hash-ne(Get-FileHash -LiteralPath (Join-Path $payload $expectedManifest)).Hash){throw 'Role integrity manifest mismatch'}
    RunInstaller $Helper ('verify "'+$app+'"')
    $settings=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registry+'\Configuration\Authentication')
    try {if($settings.GetValue('OtherSetting')-ne'PRESERVE ON UPGRADE - REMOVE ON PURGE'){throw 'Role change lost configuration'}} finally {$settings.Dispose()}
    if((Get-FileHash -LiteralPath (Join-Path $root 'keys/netlink_test')).Hash-ne$keyHash -or (Get-Acl -LiteralPath (Join-Path $root 'keys/netlink_test')).Sddl-ne$keyAcl){throw 'Role change modified key or ACL'}
}
try {
    & (Join-Path $Toolchain 'bin/makensis.exe') '/INPUTCHARSET' 'UTF8' "/DNETLINK_INSTALLER_FIXTURE" "/DFIXTURE_ID=$id" "/DFIXTURE_ROOT=$root" "/DFIXTURE_REGKEY=$registry" "/DREPOROOT=$repoRoot" "/DOUTPUT=$root" "/DPAYLOAD=$payload" (Join-Path $repoRoot 'packaging/veyon-school.nsi') | Out-Null
    if($LASTEXITCODE-ne0){throw 'Full installer fixture compilation failed'}
    $installer=Join-Path $root 'NetLink-1.0.0-alpha.31-win64-setup.exe'
    $oldHashes=@{};Get-ChildItem -LiteralPath $app -File | ForEach-Object {$oldHashes[$_.Name]=(Get-FileHash -LiteralPath $_.FullName).Hash}
    $loaded=Start-Process -FilePath $Helper -ArgumentList ('hold-image "'+$app+'"') -WindowStyle Hidden -PassThru
    try {
        $deadline=[DateTime]::UtcNow.AddSeconds(10)
        while(!(Test-Path -LiteralPath (Join-Path $root 'image-ready.txt'))){
            if($loaded.HasExited -or [DateTime]::UtcNow-ge$deadline){throw 'Loaded-image fixture did not become ready'}
            Start-Sleep -Milliseconds 50
        }
        RunInstaller $installer '/S' 3010
        foreach($name in $oldHashes.Keys){if((Get-FileHash -LiteralPath (Join-Path $app $name)).Hash-ne$oldHashes[$name]){throw "Blocked installation changed $name"}}
        if((Get-ChildItem -LiteralPath $app -File).Count-ne$oldHashes.Count -or (Test-Path -LiteralPath (Join-Path $app 'platforms'))){throw 'Blocked installation copied payload before reboot check'}
        $actions=@(Get-Content -LiteralPath (Join-Path $root 'maintenance-actions.txt'))
        if($actions -notcontains 'quiesce' -or $actions -notcontains 'check-files' -or $actions -contains 'stop' -or (Test-Path -LiteralPath (Join-Path $root 'cli-actions.txt'))){throw 'Blocked installation unregistered service or ran CLI'}
        $settings=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registry+'\Configuration\Authentication')
        if($settings.GetValue('OtherSetting')-ne'PRESERVE ON UPGRADE - REMOVE ON PURGE'){throw 'Blocked installation changed configuration'}
        $settings.Dispose()
        if((Get-FileHash -LiteralPath (Join-Path $root 'keys/netlink_test')).Hash-ne$keyHash -or (Get-Acl -LiteralPath (Join-Path $root 'keys/netlink_test')).Sddl-ne$keyAcl){throw 'Blocked installation changed a key or its ACL'}
    } finally {
        [IO.File]::WriteAllText((Join-Path $root 'image-release.txt'),'release')
        if(!$loaded.WaitForExit(5000)){$loaded.Kill();throw 'Image fixture did not stop'}
        $loaded.Dispose()
    }
    RunInstaller $installer '/S'
    CheckComponents 'Client'
    # Add Companion later without uninstalling; then preserve it on update.
    RunInstaller $installer '/S /Companion=1'
    CheckComponents 'Client' $true
    RunInstaller $installer '/S'
    CheckComponents 'Client' $true
    # Older packages have the EXE but no component metadata or prefixed labels.
    $entry=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registry+'\Uninstall',$true)
    try {$entry.DeleteValue('InstallCompanion')} finally {$entry.Dispose()}
    foreach($label in @('Configurator','Companion')) {
        Copy-Item -LiteralPath (Join-Path $app ('StartMenu/NetLink '+$label+'.lnk')) -Destination (Join-Path $app ('StartMenu/'+$label+'.lnk'))
    }
    RunInstaller $installer '/S'
    CheckComponents 'Client' $true
    RunInstaller $installer '/S /Role=Master'
    CheckComponents 'Master' $true
    # A full installation predating role metadata must not lose its Console.
    $entry=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registry+'\Uninstall',$true)
    try {$entry.DeleteValue('InstallationRole')} finally {$entry.Dispose()}
    RunInstaller $installer '/S'
    CheckComponents 'Master' $true
    # Upgrades without a role argument must retain the explicitly chosen role.
    RunInstaller $installer '/S'
    CheckComponents 'Master' $true
    $heldMaster=[IO.File]::Open((Join-Path $app 'netlink-master.exe'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {RunInstaller $installer '/S /Role=Client' 3010} finally {$heldMaster.Dispose()}
    CheckComponents 'Master' $true
    RunInstaller $installer '/S /Role=Client'
    CheckComponents 'Client' $true
    RunInstaller $installer '/S /Companion=0'
    CheckComponents 'Client'
    RunInstaller $installer '/S'
    CheckComponents 'Client'
    RunInstaller $installer '/S /Role=Master'
    CheckComponents 'Master'
    RunInstaller $installer '/S /Companion=1'
    CheckComponents 'Master' $true
    $heldCompanion=[IO.File]::Open((Join-Path $app 'netlink-companion.exe'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {RunInstaller $installer '/S /Companion=0' 3010} finally {$heldCompanion.Dispose()}
    CheckComponents 'Master' $true
    RunInstaller $installer '/S /Companion=0'
    CheckComponents 'Master'
    RunInstaller $installer '/S /Role=Client'
    CheckComponents 'Client'
    $beforeActions=@(Get-Content -LiteralPath (Join-Path $root 'maintenance-actions.txt')).Count
    RunInstaller $installer '/S /Role=Unknown' 2
    RunInstaller $installer '/S /Role=' 2
    RunInstaller $installer '/S /Companion=yes' 2
    RunInstaller $installer '/S /Companion=' 2
    if(@(Get-Content -LiteralPath (Join-Path $root 'maintenance-actions.txt')).Count-ne$beforeActions){throw 'Invalid role started maintenance'}
    CheckComponents 'Client'
    if(!(Test-Path -LiteralPath (Join-Path $app 'platforms/qwindows.dll'))){throw 'Incomplete installation was not repaired'}
    $settings=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registry+'\Configuration\Authentication')
    if($settings.GetValue('OtherSetting')-ne'PRESERVE ON UPGRADE - REMOVE ON PURGE'){throw 'Upgrade lost configuration'}
    $settings.Dispose()
    $failureMarker=Join-Path $root 'cli-failure-fixture.txt'
    [IO.File]::WriteAllText($failureMarker,'INTENTIONAL CLI FAILURE - DO NOT HANG SILENT SETUP')
    try {RunInstaller $installer '/S' 2} finally {Remove-Item -LiteralPath $failureMarker}
    RunInstaller (Join-Path $app 'Uninstall.exe') '/S /PURGE'
    WaitRemoval
    $settings=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registry+'\Configuration\Authentication')
    if(!$settings -or $settings.GetValue('OtherSetting') -or $settings.GetValue('PrivateKeyBaseDir')-ne(Join-Path $root 'keys')){throw 'Purge failed to remove settings/preserve key locations'}
    $settings.Dispose()
    RunInstaller $installer '/S'
    $settings=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($registry+'\Configuration\Authentication')
    $settings.SetValue('OtherSetting','PRESERVE ON NORMAL UNINSTALL')
    $settings.Dispose()
    RunInstaller (Join-Path $app 'Uninstall.exe') '/S'
    WaitRemoval
    $settings=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registry+'\Configuration\Authentication')
    if($settings.GetValue('OtherSetting')-ne'PRESERVE ON NORMAL UNINSTALL'){throw 'Normal uninstall lost settings'}
    $settings.Dispose()
    if((Get-FileHash -LiteralPath (Join-Path $root 'keys/netlink_test')).Hash-ne$keyHash -or (Get-Acl -LiteralPath (Join-Path $root 'keys/netlink_test')).Sddl-ne$keyAcl){throw 'Key contents/ACL changed'}
    $calls=@(Get-Content -LiteralPath (Join-Path $root 'cli-actions.txt'))
    if($calls.Count-ne($script:successfulInstalls*4) -or @($calls | Where-Object {$_-match'config export|service stop|service unregister'}).Count){throw 'Old/uninstall CLI was unexpectedly executed'}
    Write-Output 'PASS: mapped DLL and in-use optional EXEs block copying/removal; fresh Client without Companion, all four component combinations, add/remove Companion, older metadata migration, role/component persistence, invalid arguments, manifests, NetLink Start shortcuts, interrupted installation, purge/reinstall/uninstall verified; settings and key hash/ACL preserved. Service/firewall deliberately simulated.'
} finally {
    # Exact test-only HKCU subtree. Never enumerate or delete product settings.
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($registry,$false)
}
