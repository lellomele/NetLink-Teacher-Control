# Copyright (C) 2025-26 prof. Ing. Raffaele Mele — GPL-2.0-or-later
# Windows built-in vector rasterization; no additional client prerequisite.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$images=@()
foreach($size in @(16,32,48,64,128,256)) {
 $bitmap=[Drawing.Bitmap]::new($size,$size)
 $graphics=[Drawing.Graphics]::FromImage($bitmap)
 $graphics.SmoothingMode='AntiAlias';$graphics.ScaleTransform($size/256.0,$size/256.0)
 $blue=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#2563eb'))
 $green=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#34d399'))
 $path=[Drawing.Drawing2D.GraphicsPath]::new()
 $path.AddArc(0,0,96,96,180,90);$path.AddArc(160,0,96,96,270,90);$path.AddArc(160,160,96,96,0,90);$path.AddArc(0,160,96,96,90,90);$path.CloseFigure();$graphics.FillPath($blue,$path)
 $pen=[Drawing.Pen]::new([Drawing.Color]::White,18);$pen.StartCap='Round';$pen.EndCap='Round';$pen.LineJoin='Round'
 $graphics.DrawLines($pen,[Drawing.PointF[]]@([Drawing.PointF]::new(62,170),[Drawing.PointF]::new(62,64),[Drawing.PointF]::new(170,150),[Drawing.PointF]::new(170,64)))
 $graphics.FillEllipse($green,45,47,34,34)
 $graphics.FillEllipse($blue,116,115,86,86)
 $rim=[Drawing.Pen]::new($green,15);$graphics.DrawEllipse($rim,116,115,86,86)
 $rim.Width=19;$rim.StartCap='Round';$rim.EndCap='Round';$graphics.DrawLine($rim,190,190,225,225)
 $pen.Width=10;$graphics.DrawLine($pen,140,158,178,158);$graphics.DrawLine($pen,159,139,159,177)
 $output=Join-Path $PSScriptRoot "companion$size.png"
 $bitmap.Save($output,[Drawing.Imaging.ImageFormat]::Png)
 $images+=,@($size,[IO.File]::ReadAllBytes($output))
 $path.Dispose();$pen.Dispose();$rim.Dispose();$blue.Dispose();$green.Dispose();$graphics.Dispose();$bitmap.Dispose()
}
$stream=[IO.MemoryStream]::new();$writer=[IO.BinaryWriter]::new($stream)
$writer.Write([UInt16]0);$writer.Write([UInt16]1);$writer.Write([UInt16]$images.Count);$offset=6+16*$images.Count
foreach($entry in $images){$dimension=if($entry[0]-eq256){0}else{$entry[0]};$writer.Write([byte]$dimension);$writer.Write([byte]$dimension);$writer.Write([UInt16]0);$writer.Write([UInt16]1);$writer.Write([UInt16]32);$writer.Write([UInt32]$entry[1].Length);$writer.Write([UInt32]$offset);$offset+=$entry[1].Length}
foreach($entry in $images){$writer.Write([byte[]]$entry[1])}
$writer.Flush();[IO.File]::WriteAllBytes((Join-Path $PSScriptRoot 'netlink-companion.ico'),$stream.ToArray());$writer.Dispose();$stream.Dispose()
