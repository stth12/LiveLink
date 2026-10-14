$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$repository = (Resolve-Path "$PSScriptRoot/..").Path
$iconFolder = Join-Path $repository 'ios/LiveLink/Assets.xcassets/AppIcon.appiconset'
New-Item -ItemType Directory -Path $iconFolder -Force | Out-Null
function Draw-LiveLinkIcon([int]$size, [string]$path) {
  $bitmap = [System.Drawing.Bitmap]::new($size,$size)
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $graphics.Clear([System.Drawing.ColorTranslator]::FromHtml('#08080B'))
  $pen = [System.Drawing.Pen]::new([System.Drawing.ColorTranslator]::FromHtml('#FF2D8D'),[single]($size*18/256))
  $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
  $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
  $positions = @(@(58,96,160),@(93,70,186),@(128,42,214),@(163,70,186),@(198,96,160))
  foreach ($line in $positions) { $graphics.DrawLine($pen,[single]($line[0]*$size/256),[single]($line[1]*$size/256),[single]($line[0]*$size/256),[single]($line[2]*$size/256)) }
  $bitmap.Save($path,[System.Drawing.Imaging.ImageFormat]::Png)
  $pen.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
}
Draw-LiveLinkIcon 1024 (Join-Path $iconFolder 'LiveLink.png')
Draw-LiveLinkIcon 256 (Join-Path $repository 'installer/assets/LiveLink.png')
@'
{"images":[{"filename":"LiveLink.png","idiom":"universal","platform":"ios","size":"1024x1024"}],"info":{"author":"xcode","version":1}}
'@ | Set-Content -LiteralPath (Join-Path $iconFolder 'Contents.json') -Encoding utf8
'{"info":{"author":"xcode","version":1}}' | Set-Content -LiteralPath (Join-Path $repository 'ios/LiveLink/Assets.xcassets/Contents.json') -Encoding utf8
$png = [System.IO.File]::ReadAllBytes((Join-Path $repository 'installer/assets/LiveLink.png'))
$stream = [System.IO.File]::Create((Join-Path $repository 'installer/assets/LiveLink.ico'))
$writer = [System.IO.BinaryWriter]::new($stream)
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]1)
$writer.Write([byte]0); $writer.Write([byte]0); $writer.Write([byte]0); $writer.Write([byte]0)
$writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([uint32]$png.Length); $writer.Write([uint32]22); $writer.Write($png)
$writer.Dispose()
$banner = [System.Drawing.Bitmap]::new(164,314)
$canvas = [System.Drawing.Graphics]::FromImage($banner)
$canvas.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$canvas.Clear([System.Drawing.ColorTranslator]::FromHtml('#08080B'))
$titleFont = [System.Drawing.Font]::new('Segoe UI',24,[System.Drawing.FontStyle]::Bold)
$captionFont = [System.Drawing.Font]::new('Segoe UI',10)
$titleBrush = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#F5F5F7'))
$captionBrush = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#92929D'))
$canvas.DrawString("LIVE`nLINK",$titleFont,$titleBrush,14,24)
$canvas.DrawString("Your Sound.`nEverywhere.",$captionFont,$captionBrush,18,118)
$iconImage = [System.Drawing.Image]::FromFile((Join-Path $repository 'installer/assets/LiveLink.png'))
$canvas.DrawImage($iconImage,18,176,128,128)
$banner.Save((Join-Path $repository 'installer/assets/Wizard.bmp'),[System.Drawing.Imaging.ImageFormat]::Bmp)
$small = [System.Drawing.Bitmap]::new(55,55)
$smallCanvas = [System.Drawing.Graphics]::FromImage($small)
$smallCanvas.DrawImage($iconImage,0,0,55,55)
$small.Save((Join-Path $repository 'installer/assets/WizardSmall.bmp'),[System.Drawing.Imaging.ImageFormat]::Bmp)
$smallCanvas.Dispose();$small.Dispose();$iconImage.Dispose();$canvas.Dispose();$banner.Dispose();$titleFont.Dispose();$captionFont.Dispose();$titleBrush.Dispose();$captionBrush.Dispose()
