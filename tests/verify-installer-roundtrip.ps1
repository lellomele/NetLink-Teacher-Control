param([Parameter(Mandatory=$true)][string]$BuildRoot,[Parameter(Mandatory=$true)][string]$Toolchain,[Parameter(Mandatory=$true)][string]$Helper,[Parameter(Mandatory=$true)][string]$Cli)
$ErrorActionPreference='Stop'
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
$registry='Software\NetLinkInstallerTests\'+(Split-Path $root -Leaf)
$settings=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($registry+'\Configuration\Authentication')
$settings.SetValue('PrivateKeyBaseDir',(Join-Path $root 'keys'))
$settings.SetValue('PublicKeyBaseDir',(Join-Path $root 'keys'))
$settings.SetValue('OtherSetting','PRESERVE ON UPGRADE - REMOVE ON PURGE')
$settings.Dispose()
function RunInstaller([string]$file,[string]$arguments,[int]$expectedExit=0) {
    $process=Start-Process -FilePath $file -ArgumentList $arguments -WindowStyle Hidden -PassThru
    try {if(!$process.WaitForExit(60000)){$process.Kill();throw "Installer fixture deadline exceeded: $file $arguments"}; if($process.ExitCode-ne$expectedExit){$detail=if(Test-Path -LiteralPath (Join-Path $root 'fixture-error.txt')){Get-Content -LiteralPath (Join-Path $root 'fixture-error.txt') -Raw}else{'no helper error'}; throw "Installer fixture failed: $($process.ExitCode), expected $expectedExit`: $detail"}}finally{$process.Dispose()}
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
function CheckRole([string]$role) {
    $isMaster=$role-eq'Master'
    foreach($relative in @('netlink-master.exe','StartMenu/Console docente.lnk')) {
        if((Test-Path -LiteralPath (Join-Path $app $relative))-ne$isMaster){throw "Wrong $role payload/shortcut: $relative"}
    }
    foreach($relative in @('netlink-configurator.exe','netlink-companion.exe','StartMenu/Configurator.lnk','StartMenu/Companion.lnk')) {
        if(!(Test-Path -LiteralPath (Join-Path $app $relative))){throw "Missing $role component: $relative"}
    }
    $shell=New-Object -ComObject WScript.Shell
    try {
        $links=@{'Configurator'='netlink-configurator.exe';'Companion'='netlink-companion.exe'}
        if($isMaster){$links['Console docente']='netlink-master.exe'}
        foreach($label in $links.Keys) {
            $shortcut=$shell.CreateShortcut((Join-Path $app ('StartMenu/'+$label+'.lnk')))
            try {
                if($shortcut.TargetPath-ne(Join-Path $app $links[$label])){throw "Wrong shortcut target: $label"}
            } finally {[void][Runtime.InteropServices.Marshal]::ReleaseComObject($shortcut)}
        }
    } finally {[void][Runtime.InteropServices.Marshal]::ReleaseComObject($shell)}
    if(Test-Path -LiteralPath (Join-Path $app 'payload-client.sha256')){throw 'Packaging-only Client manifest was installed'}
    $entry=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registry+'\Uninstall')
    try {
        if(!$entry -or $entry.GetValue('InstallationRole')-ne$role){throw 'Installed role not persisted'}
        if($entry.GetValue('DisplayIcon')-ne(Join-Path $app 'netlink-configurator.exe')){throw 'Uninstall icon refers to optional Master'}
    } finally {if($entry){$entry.Dispose()}}
    $expectedManifest=if($isMaster){'payload.sha256'}else{'payload-client.sha256'}
    if((Get-FileHash -LiteralPath (Join-Path $app 'payload.sha256')).Hash-ne(Get-FileHash -LiteralPath (Join-Path $payload $expectedManifest)).Hash){throw 'Role integrity manifest mismatch'}
    RunInstaller $Helper ('verify "'+$app+'"')
    $settings=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registry+'\Configuration\Authentication')
    try {if($settings.GetValue('OtherSetting')-ne'PRESERVE ON UPGRADE - REMOVE ON PURGE'){throw 'Role change lost configuration'}} finally {$settings.Dispose()}
    if((Get-FileHash -LiteralPath (Join-Path $root 'keys/netlink_test')).Hash-ne$keyHash -or (Get-Acl -LiteralPath (Join-Path $root 'keys/netlink_test')).Sddl-ne$keyAcl){throw 'Role change modified key or ACL'}
}
try {
    & (Join-Path $Toolchain 'bin/makensis.exe') '/INPUTCHARSET' 'UTF8' "/DNETLINK_INSTALLER_FIXTURE" "/DFIXTURE_ID=$id" "/DFIXTURE_ROOT=$root" "/DFIXTURE_REGKEY=$registry" "/DREPOROOT=$repoRoot" "/DOUTPUT=$root" "/DPAYLOAD=$payload" (Join-Path $repoRoot 'packaging/veyon-school.nsi') | Out-Null
    if($LASTEXITCODE-ne0){throw 'Full installer fixture compilation failed'}
    $installer=Join-Path $root 'NetLink-1.0.0-alpha.22-win64-setup.exe'
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
    CheckRole 'Client'
    RunInstaller $installer '/S /Role=Master'
    CheckRole 'Master'
    # A full installation predating role metadata must not lose its Console.
    $entry=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registry+'\Uninstall',$true)
    try {$entry.DeleteValue('InstallationRole')} finally {$entry.Dispose()}
    RunInstaller $installer '/S'
    CheckRole 'Master'
    # Upgrades without a role argument must retain the explicitly chosen role.
    RunInstaller $installer '/S'
    CheckRole 'Master'
    $heldMaster=[IO.File]::Open((Join-Path $app 'netlink-master.exe'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {RunInstaller $installer '/S /Role=Client' 3010} finally {$heldMaster.Dispose()}
    CheckRole 'Master'
    RunInstaller $installer '/S /Role=Client'
    CheckRole 'Client'
    RunInstaller $installer '/S'
    CheckRole 'Client'
    $beforeActions=@(Get-Content -LiteralPath (Join-Path $root 'maintenance-actions.txt')).Count
    RunInstaller $installer '/S /Role=Unknown' 2
    RunInstaller $installer '/S /Role=' 2
    if(@(Get-Content -LiteralPath (Join-Path $root 'maintenance-actions.txt')).Count-ne$beforeActions){throw 'Invalid role started maintenance'}
    CheckRole 'Client'
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
    if($calls.Count-ne28 -or @($calls | Where-Object {$_-match'config export|service stop|service unregister'}).Count){throw 'Old/uninstall CLI was unexpectedly executed'}
    Write-Output 'PASS: mapped DLL and in-use Master EXE block copying/removal; fresh Client, Client/Master transitions, role persistence, invalid roles, manifests, Start shortcuts, interrupted installation, purge/reinstall/uninstall verified; settings and key hash/ACL preserved. Service/firewall deliberately simulated.'
} finally {
    # Exact test-only HKCU subtree. Never enumerate or delete product settings.
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($registry,$false)
}
