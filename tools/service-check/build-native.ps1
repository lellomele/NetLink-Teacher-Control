param([string]$Toolchain='C:/msys64/mingw64')
$ErrorActionPreference='Stop'
$root=$PSScriptRoot;$build=Join-Path $root 'build/native'
$source=Join-Path $root 'native'
if(!(Test-Path -LiteralPath $source)){
 $source=Join-Path $root '../../packaging'
 if(!(Test-Path -LiteralPath (Join-Path $source 'StartupCheck.cpp'))){$source=Join-Path $root '../VeyonSchool/packaging'}
}
$source=(Resolve-Path -LiteralPath $source).Path
$branding=if(Test-Path -LiteralPath (Join-Path $source 'branding')){$source}else{Split-Path $source}
New-Item -ItemType Directory -Force -Path $build | Out-Null
$rc=[IO.File]::ReadAllText((Join-Path $source 'netlink-startup-check.rc.in')).Replace('@CMAKE_SOURCE_DIR@',$branding.Replace('\','/')).Replace('@VERSION_MAJOR@','4').Replace('@VERSION_MINOR@','7').Replace('@VERSION_PATCH@','5').Replace('@VERSION_BUILD@','38').Replace('@VERSION_STRING@','4.7.5-netlink.1.0.0-alpha.38')
$resource=Join-Path $build 'startup-check.rc';[IO.File]::WriteAllText($resource,$rc,[Text.UTF8Encoding]::new($false))
$object=Join-Path $build 'resource.o';$output=Join-Path $build 'netlink-startup-check.exe'
& (Join-Path $Toolchain 'bin/windres.exe') $resource $object
if($LASTEXITCODE-ne0){throw 'Native resource compilation failed'}
& (Join-Path $Toolchain 'bin/g++.exe') '-std=c++17' '-O2' '-static' '-municode' '-DUNICODE' '-D_UNICODE' '-D_WIN32_WINNT=0x0601' '-DNOMINMAX' (Join-Path $source 'StartupCheck.cpp') $object '-o' $output '-ladvapi32' '-lshell32' '-lole32' '-loleaut32' '-luuid' '-ltaskschd'
if($LASTEXITCODE-ne0){throw 'Native helper compilation failed'}
Write-Output $output
