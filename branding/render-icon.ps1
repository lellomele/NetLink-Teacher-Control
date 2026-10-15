# Copyright (C) 2025-26 prof. Ing. Raffaele Mele
# Render the NetLink vector design with the built-in Windows drawing API.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$brandingRoot = $PSScriptRoot
$repoRoot = Split-Path $brandingRoot
$pngs = @()
foreach ($size in @(16,22,24,32,48,64,128,256)) {
    $bitmap = [Drawing.Bitmap]::new($size,$size)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.ScaleTransform($size/256.0,$size/256.0)
    $blue = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#2563eb'))
    $white = [Drawing.SolidBrush]::new([Drawing.Color]::White)
    $teal = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#34d399'))
    $rounded = [Drawing.Drawing2D.GraphicsPath]::new()
    $rounded.AddArc(0,0,96,96,180,90); $rounded.AddArc(160,0,96,96,270,90)
    $rounded.AddArc(160,160,96,96,0,90); $rounded.AddArc(0,160,96,96,90,90); $rounded.CloseFigure()
    $graphics.FillPath($blue,$rounded)
    $link=[Drawing.Pen]::new($white,22);$link.StartCap='Round';$link.EndCap='Round';$link.LineJoin='Round'
    $graphics.DrawLines($link,[Drawing.PointF[]]@([Drawing.PointF]::new(70,180),[Drawing.PointF]::new(70,76),[Drawing.PointF]::new(186,180),[Drawing.PointF]::new(186,76)))
    $graphics.FillEllipse($teal,51,57,38,38);$graphics.FillEllipse($teal,167,161,38,38)
    $path=Join-Path $brandingRoot "icon$size.png";$bitmap.Save($path,[Drawing.Imaging.ImageFormat]::Png)
    if($size -in @(16,22,32,64)) {Copy-Item -LiteralPath $path -Destination (Join-Path $repoRoot "core/resources/icon$size.png") -Force}
    if($size -in @(16,32,48,64,128,256)) {$pngs+=,@($size,[IO.File]::ReadAllBytes($path))}
    $link.Dispose();$rounded.Dispose()
    $blue.Dispose();$white.Dispose();$teal.Dispose();$graphics.Dispose();$bitmap.Dispose()
}
$stream=[IO.MemoryStream]::new();$writer=[IO.BinaryWriter]::new($stream)
$writer.Write([UInt16]0);$writer.Write([UInt16]1);$writer.Write([UInt16]$pngs.Count)
$offset=6+16*$pngs.Count
foreach($png in $pngs){$dimension=if($png[0]-eq256){0}else{$png[0]};$writer.Write([byte]$dimension);$writer.Write([byte]$dimension);$writer.Write([UInt16]0);$writer.Write([UInt16]1);$writer.Write([UInt16]32);$writer.Write([UInt32]$png[1].Length);$writer.Write([UInt32]$offset);$offset+=$png[1].Length}
foreach($png in $pngs){$writer.Write([byte[]]$png[1])}
$writer.Flush();[IO.File]::WriteAllBytes((Join-Path $brandingRoot 'netlink.ico'),$stream.ToArray());$writer.Dispose();$stream.Dispose()
Copy-Item -LiteralPath (Join-Path $brandingRoot 'netlink.ico') -Destination (Join-Path $repoRoot 'master/data/veyon-master.ico') -Force
Copy-Item -LiteralPath (Join-Path $brandingRoot 'netlink.ico') -Destination (Join-Path $repoRoot 'configurator/data/veyon-configurator.ico') -Force
# Splash is also generated, with no fonts or rendering tools to install on clients.
$splash=[Drawing.Bitmap]::new(700,220);$surface=[Drawing.Graphics]::FromImage($splash)
$surface.Clear([Drawing.ColorTranslator]::FromHtml('#f3f5f9'))
$logo=[Drawing.Image]::FromFile((Join-Path $brandingRoot 'icon128.png'));$surface.DrawImage($logo,28,44,128,128)
$ink=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#202c3d'))
$title=[Drawing.Font]::new('Segoe UI',30,[Drawing.FontStyle]::Bold)
$body=[Drawing.Font]::new('Segoe UI',12)
$surface.DrawString('NetLink',$title,$ink,182,48)
$surface.DrawString("Console docente - gestione e controllo dell'aula",$body,$ink,185,110)
$surface.DrawString('Copyright © 2025-26 prof. Ing. Raffaele Mele',$body,$ink,185,145)
$splash.Save((Join-Path $repoRoot 'master/resources/splash.png'),[Drawing.Imaging.ImageFormat]::Png)
$logo.Dispose();$ink.Dispose();$title.Dispose();$body.Dispose();$surface.Dispose();$splash.Dispose()
