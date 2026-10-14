# Render the repository-native vector design. Windows built-in drawing API only.
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
    $blue = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#17345d'))
    $white = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#f4f8ff'))
    $teal = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#20b6aa'))
    $gold = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#ffc857'))
    $rounded = [Drawing.Drawing2D.GraphicsPath]::new()
    $rounded.AddArc(0,0,96,96,180,90); $rounded.AddArc(160,0,96,96,270,90)
    $rounded.AddArc(160,160,96,96,0,90); $rounded.AddArc(0,160,96,96,90,90); $rounded.CloseFigure()
    $graphics.FillPath($blue,$rounded)
    $graphics.FillRectangle($white,44,80,168,102); $graphics.FillRectangle($teal,54,90,148,66)
    $stand = [Drawing.Pen]::new($white,12); $stand.StartCap='Round'; $stand.EndCap='Round'
    $graphics.DrawLine($stand,128,180,128,204);$graphics.DrawLine($stand,90,206,166,206)
    $shield = [Drawing.Drawing2D.GraphicsPath]::new()
    $shield.AddLines([Drawing.PointF[]]@([Drawing.PointF]::new(128,48),[Drawing.PointF]::new(176,66),[Drawing.PointF]::new(176,110)))
    $shield.AddBezier(176,110,176,141,152,162,128,174);$shield.AddBezier(128,174,104,162,80,141,80,110)
    $shield.AddLine(80,110,80,66);$shield.CloseFigure()
    $outline=[Drawing.Pen]::new($blue,8);$graphics.FillPath($gold,$shield);$graphics.DrawPath($outline,$shield)
    $check=[Drawing.Pen]::new($blue,12);$check.StartCap='Round';$check.EndCap='Round';$check.LineJoin='Round'
    $graphics.DrawLines($check,[Drawing.PointF[]]@([Drawing.PointF]::new(105,110),[Drawing.PointF]::new(121,126),[Drawing.PointF]::new(151,93)))
    $path=Join-Path $brandingRoot "icon$size.png";$bitmap.Save($path,[Drawing.Imaging.ImageFormat]::Png)
    if($size -in @(16,22,32,64)) {Copy-Item -LiteralPath $path -Destination (Join-Path $repoRoot "core/resources/icon$size.png") -Force}
    if($size -in @(16,32,48,64,128,256)) {$pngs+=,@($size,[IO.File]::ReadAllBytes($path))}
    $check.Dispose();$outline.Dispose();$stand.Dispose();$shield.Dispose();$rounded.Dispose()
    $blue.Dispose();$white.Dispose();$teal.Dispose();$gold.Dispose();$graphics.Dispose();$bitmap.Dispose()
}
$stream=[IO.MemoryStream]::new();$writer=[IO.BinaryWriter]::new($stream)
$writer.Write([UInt16]0);$writer.Write([UInt16]1);$writer.Write([UInt16]$pngs.Count)
$offset=6+16*$pngs.Count
foreach($png in $pngs){$dimension=if($png[0]-eq256){0}else{$png[0]};$writer.Write([byte]$dimension);$writer.Write([byte]$dimension);$writer.Write([UInt16]0);$writer.Write([UInt16]1);$writer.Write([UInt16]32);$writer.Write([UInt32]$png[1].Length);$writer.Write([UInt32]$offset);$offset+=$png[1].Length}
foreach($png in $pngs){$writer.Write([byte[]]$png[1])}
$writer.Flush();[IO.File]::WriteAllBytes((Join-Path $brandingRoot 'veyon-school.ico'),$stream.ToArray());$writer.Dispose();$stream.Dispose()
Copy-Item -LiteralPath (Join-Path $brandingRoot 'veyon-school.ico') -Destination (Join-Path $repoRoot 'master/data/veyon-master.ico') -Force
Copy-Item -LiteralPath (Join-Path $brandingRoot 'veyon-school.ico') -Destination (Join-Path $repoRoot 'configurator/data/veyon-configurator.ico') -Force
