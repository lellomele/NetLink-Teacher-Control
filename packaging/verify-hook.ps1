param([Parameter(Mandatory=$true)][string]$HookPath,[string]$Toolchain='C:/msys64/mingw64')
$ErrorActionPreference='Stop'
$hook=(Resolve-Path -LiteralPath $HookPath).Path
$objectDump=Join-Path $Toolchain 'bin/objdump.exe'
$details=@(& $objectDump -p $hook)
if($LASTEXITCODE -ne 0){throw 'Cannot inspect injected hook DLL'}
$headers=$details -join "`n"
if($headers -notmatch 'file format pei-x86-64'){throw 'Injected hook must be a Windows x64 PE'}
$imports=@($details | ForEach-Object {if($_ -match 'DLL Name:\s*(\S+)'){$Matches[1].ToUpperInvariant()}})
if(!$imports.Count){throw 'Hook import inventory is empty'}
# This DLL is loaded in unrelated desktop processes. It must not pull in any
# CRT, C++ runtime, pthread, Qt, compiler support or application-private DLL.
foreach($dependency in $imports){if($dependency -notin @('KERNEL32.DLL','USER32.DLL')){throw "Unsafe injected hook dependency: $dependency"}}
$tls=@($details | Where-Object {$_ -match '^Entry 9\s+([a-fA-F0-9]+)\s+([a-fA-F0-9]+)\s+Thread Storage Directory'} )
if($tls.Count -ne 1 -or $tls[0] -notmatch '^Entry 9\s+0+\s+0+\s+Thread Storage Directory'){throw 'Injected hook contains TLS initialization or an unreadable TLS directory'}
$delay=@($details | Where-Object {$_ -match '^Entry d\s+([a-fA-F0-9]+)\s+([a-fA-F0-9]+)\s+Delay Import Directory'})
if($delay.Count -ne 1 -or $delay[0] -notmatch '^Entry d\s+0+\s+0+\s+Delay Import Directory'){throw 'Injected hook contains delayed imports or an unreadable import directory'}
foreach($export in @('HooksType','SetHooks','UnSetHooks','SetKeyboardFilterHook','SetMouseFilterHook','GetHookDiagnostics')){
    if($headers -notmatch ('(?m)\]\s+(?:[a-fA-F0-9]+\s+)?'+[regex]::Escape($export)+'\s*$')){throw "Missing hook ABI export: $export"}
}
Write-Output ('Verified injected hook: KERNEL32/USER32 only, no TLS/delayed runtime, required exports present ('+([IO.Path]::GetFileName($hook))+').')
