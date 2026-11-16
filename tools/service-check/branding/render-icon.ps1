$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$pngs=@()
foreach($size in @(16,24,32,48,64,128,256)) {
 $bitmap=[Drawing.Bitmap]::new($size,$size);$g=[Drawing.Graphics]::FromImage($bitmap);$g.SmoothingMode='AntiAlias';$g.ScaleTransform($size/256.0,$size/256.0)
 $p=[Drawing.Drawing2D.GraphicsPath]::new();foreach($arc in @(@(8,8,180),@(144,8,270),@(144,144,0),@(8,144,90))){$p.AddArc($arc[0],$arc[1],104,104,$arc[2],90)};$p.CloseFigure();$g.FillPath([Drawing.Brushes]::RoyalBlue,$p);$p.Dispose()
 $white=[Drawing.Pen]::new([Drawing.Color]::White,20);$white.StartCap='Round';$white.EndCap='Round';$white.LineJoin='Round';$g.DrawLines($white,[Drawing.PointF[]]@([Drawing.PointF]::new(58,178),[Drawing.PointF]::new(58,69),[Drawing.PointF]::new(140,172),[Drawing.PointF]::new(140,69)))
 $shield=[Drawing.PointF[]]@([Drawing.PointF]::new(172,113),[Drawing.PointF]::new(225,134),[Drawing.PointF]::new(225,172),[Drawing.PointF]::new(212,204),[Drawing.PointF]::new(172,228),[Drawing.PointF]::new(132,204),[Drawing.PointF]::new(119,172),[Drawing.PointF]::new(119,134));$brush=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#34d399'));$g.FillPolygon($brush,$shield);$outline=[Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml('#174d73'),6);$g.DrawPolygon($outline,$shield);$white.Width=12;$g.DrawLines($white,[Drawing.PointF[]]@([Drawing.PointF]::new(146,169),[Drawing.PointF]::new(164,187),[Drawing.PointF]::new(198,149)))
 $path=Join-Path $PSScriptRoot "icon-$size.png";$bitmap.Save($path,[Drawing.Imaging.ImageFormat]::Png);$pngs+=,@($size,[IO.File]::ReadAllBytes($path));$white.Dispose();$brush.Dispose();$outline.Dispose();$g.Dispose();$bitmap.Dispose()
}
$stream=[IO.MemoryStream]::new();$writer=[IO.BinaryWriter]::new($stream);$writer.Write([UInt16]0);$writer.Write([UInt16]1);$writer.Write([UInt16]$pngs.Count);$offset=6+16*$pngs.Count
foreach($png in $pngs){$dimension=if($png[0]-eq256){0}else{$png[0]};$writer.Write([byte]$dimension);$writer.Write([byte]$dimension);$writer.Write([UInt16]0);$writer.Write([UInt16]1);$writer.Write([UInt16]32);$writer.Write([UInt32]$png[1].Length);$writer.Write([UInt32]$offset);$offset+=$png[1].Length};foreach($png in $pngs){$writer.Write([byte[]]$png[1])};$writer.Flush();[IO.File]::WriteAllBytes((Join-Path $PSScriptRoot 'service-check.ico'),$stream.ToArray());$writer.Dispose();$stream.Dispose()
