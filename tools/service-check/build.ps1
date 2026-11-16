param([string]$NativeHelper='')
$ErrorActionPreference='Stop'
$root=$PSScriptRoot;$build=Join-Path $root 'build';$dist=Join-Path $root 'dist'
New-Item -ItemType Directory -Path $build,$dist -Force | Out-Null
& (Join-Path $root 'branding/render-icon.ps1')
$compiler=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
if(!$NativeHelper){$NativeHelper=& (Join-Path $root 'build-native.ps1') | Select-Object -Last 1}
$native=(Resolve-Path -LiteralPath $NativeHelper).Path
$digest=(Get-FileHash -LiteralPath $native -Algorithm SHA256).Hash
$generated=Join-Path $build 'StartupPayload.cs'
[IO.File]::WriteAllText($generated,'namespace NetLinkServiceCheck { static class StartupPayload { public const string Sha256="'+$digest+'"; } }',[Text.UTF8Encoding]::new($false))
$sources=@('src/Model.cs','src/WindowsHost.cs','src/StartupController.cs','src/Program.cs') | ForEach-Object {Join-Path $root $_}
$sources+= $generated
$references=@('/reference:System.dll','/reference:System.Core.dll','/reference:System.Drawing.dll','/reference:System.Windows.Forms.dll','/reference:System.Web.Extensions.dll')
$flags=@('/nologo','/optimize+','/platform:x64','/utf8output','/warn:4',("/resource:"+$native+',NetLink.StartupCheck'))
$output=Join-Path $dist 'NetLink-Service-Check.exe'
& $compiler @flags @references '/target:winexe' ("/win32manifest:"+(Join-Path $root 'app.manifest')) ("/win32icon:"+(Join-Path $root 'branding/service-check.ico')) ("/out:"+$output) @sources
if($LASTEXITCODE-ne0){throw 'Application build failed'}
$tests=Join-Path $build 'ServiceCheckTests.exe'
& $compiler @flags @references '/target:exe' '/main:Tests' ("/win32icon:"+(Join-Path $root 'branding/service-check.ico')) ("/out:"+$tests) @sources (Join-Path $root 'tests/Tests.cs')
if($LASTEXITCODE-ne0){throw 'Test build failed'}
& $tests $build *> (Join-Path $build 'test-results.txt')
if($LASTEXITCODE-ne0){Get-Content -LiteralPath (Join-Path $build 'test-results.txt');throw 'Isolated tests failed'}
Get-Content -LiteralPath (Join-Path $build 'test-results.txt') -Tail 5
Get-Item -LiteralPath $output | Select-Object Name,Length
