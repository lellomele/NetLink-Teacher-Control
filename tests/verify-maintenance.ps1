param([Parameter(Mandatory=$true)][string]$Executable,[Parameter(Mandatory=$true)][string]$BuildRoot)
$ErrorActionPreference='Stop'
$base=(Resolve-Path -LiteralPath $BuildRoot).Path
$root=[IO.Path]::GetFullPath((Join-Path $base ('maintenance-'+[Guid]::NewGuid().ToString('N'))))
if(!$root.StartsWith($base+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe fixture root'}
New-Item -ItemType Directory -Path (Join-Path $root 'junction-case'),(Join-Path $root 'outside') | Out-Null
[IO.File]::WriteAllText((Join-Path $root 'outside/must-survive.txt'),'OUTSIDE TARGET - DO NOT REMOVE')
New-Item -ItemType Junction -Path (Join-Path $root 'junction-case/redirect') -Target (Join-Path $root 'outside') | Out-Null
& $Executable $root
if($LASTEXITCODE-ne0){throw 'Native maintenance tests failed'}
if(!(Test-Path -LiteralPath (Join-Path $root 'outside/must-survive.txt'))){throw 'Junction target was modified'}
