# Generates res/kb_lay.ico — dual-tone A / Я tray + app icon.
# Run: powershell.exe -NoProfile -File tools\make_icon.ps1
Set-StrictMode -Version 3
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$root = Split-Path (Split-Path $PSScriptRoot -Parent) -ErrorAction SilentlyContinue
if (-not $root) { $root = Split-Path $PSScriptRoot -Parent }
# tools/ -> repo root
$repo = Resolve-Path (Join-Path $PSScriptRoot '..')
$outDir = Join-Path $repo 'res'
$tmp = Join-Path $env:TEMP 'kb_lay_icon'
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$plate = [Drawing.Color]::FromArgb(255, 14, 16, 24)
$edge  = [Drawing.Color]::FromArgb(255, 42, 48, 72)
$cyan  = [Drawing.Color]::FromArgb(255, 45, 225, 255)
$pink  = [Drawing.Color]::FromArgb(255, 255, 79, 154)
$white = [Drawing.Color]::FromArgb(255, 244, 247, 255)

$bitsA = @(
    '00100',
    '01010',
    '10001',
    '10001',
    '11111',
    '10001',
    '10001'
)
$bitsYa = @(
    '01111',
    '10001',
    '10001',
    '01111',
    '10100',
    '10010',
    '10001'
)

function Test-RoundRect([int]$x, [int]$y, [int]$size, [int]$pad, [int]$cr) {
    $l = $pad; $t = $pad; $r = $size - 1 - $pad; $b = $size - 1 - $pad
    if ($x -lt $l -or $x -gt $r -or $y -lt $t -or $y -gt $b) { return $false }
    $cx = $x; $cy = $y
    $ok = {
        param($px, $py, $ox, $oy)
        $dx = $px - $ox; $dy = $py - $oy
        return ($dx * $dx + $dy * $dy) -le ($cr * $cr + $cr)
    }
    if ($x -lt $l + $cr -and $y -lt $t + $cr) { return (& $ok $x $y ($l + $cr) ($t + $cr)) }
    if ($x -gt $r - $cr -and $y -lt $t + $cr) { return (& $ok $x $y ($r - $cr) ($t + $cr)) }
    if ($x -lt $l + $cr -and $y -gt $b - $cr) { return (& $ok $x $y ($l + $cr) ($b - $cr)) }
    if ($x -gt $r - $cr -and $y -gt $b - $cr) { return (& $ok $x $y ($r - $cr) ($b - $cr)) }
    return $true
}

function Stamp-Bits([Drawing.Bitmap]$bmp, [int]$ox, [int]$oy, [string[]]$bits, [Drawing.Color]$col, [int]$scale) {
    for ($row = 0; $row -lt $bits.Length; $row++) {
        $line = $bits[$row]
        for ($colx = 0; $colx -lt $line.Length; $colx++) {
            if ($line[$colx] -ne '1') { continue }
            for ($sy = 0; $sy -lt $scale; $sy++) {
                for ($sx = 0; $sx -lt $scale; $sx++) {
                    $xx = $ox + $colx * $scale + $sx
                    $yy = $oy + $row * $scale + $sy
                    if ($xx -ge 0 -and $yy -ge 0 -and $xx -lt $bmp.Width -and $yy -lt $bmp.Height) {
                        $bmp.SetPixel($xx, $yy, $col)
                    }
                }
            }
        }
    }
}

function New-PixelIcon([int]$size) {
    $bmp = New-Object Drawing.Bitmap $size, $size, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $pad = [Math]::Max(1, [int][Math]::Floor($size / 16.0))
    $cr = [Math]::Max(2, [int][Math]::Floor($size / 6.0))
    $mid = [int][Math]::Floor($size / 2.0)

    for ($y = 0; $y -lt $size; $y++) {
        for ($x = 0; $x -lt $size; $x++) {
            if (-not (Test-RoundRect $x $y $size $pad $cr)) { continue }
            $isEdge = ($x -eq $pad -or $y -eq $pad -or $x -eq ($size - 1 - $pad) -or $y -eq ($size - 1 - $pad))
            if ($isEdge) {
                $bmp.SetPixel($x, $y, $edge)
            } else {
                $bmp.SetPixel($x, $y, $plate)
            }
        }
    }

    $scale = [Math]::Max(1, [int][Math]::Floor($size / 16.0))
    $letterH = 7 * $scale
    $letterW = 5 * $scale
    $oy = [int](($size - $letterH) / 2) + [int]($scale)
    $oxA = $pad + [int](($mid - $pad - $letterW) / 2)
    $oxYa = $mid + [int](($size - $pad - $mid - $letterW) / 2)
    Stamp-Bits $bmp $oxA $oy $bitsA $cyan $scale
    Stamp-Bits $bmp $oxYa $oy $bitsYa $pink $scale
    return $bmp
}

function New-RoundPath([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object Drawing.Drawing2D.GraphicsPath
    $d = [Math]::Min($r * 2, [Math]::Min($w, $h))
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function Stamp-BitsGlow([Drawing.Graphics]$g, [float]$ox, [float]$oy, [string[]]$bits, [Drawing.Color]$col, [float]$cell, [float]$gap) {
    $core = New-Object Drawing.SolidBrush $col
    $glow = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(70, $col))
    $rr = [Math]::Max(0.6, $cell * 0.28)
    for ($row = 0; $row -lt $bits.Length; $row++) {
        $line = $bits[$row]
        for ($colx = 0; $colx -lt $line.Length; $colx++) {
            if ($line[$colx] -ne '1') { continue }
            $x = $ox + $colx * ($cell + $gap)
            $y = $oy + $row * ($cell + $gap)
            if ($gap -gt 0.2) {
                $g.FillEllipse($glow, $x - $gap, $y - $gap, $cell + 2 * $gap, $cell + 2 * $gap)
            }
            $p = New-RoundPath $x $y $cell $cell $rr
            $g.FillPath($core, $p)
            $p.Dispose()
        }
    }
    $core.Dispose()
    $glow.Dispose()
}

function New-HiResIcon([int]$size) {
    $bmp = New-Object Drawing.Bitmap $size, $size, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.Clear([Drawing.Color]::Transparent)

    $pad = $size * 0.06
    $body = $size - 2 * $pad
    $rad = $body * 0.28
    $path = New-RoundPath $pad $pad $body $body $rad

    $plateBrush = New-Object Drawing.SolidBrush $plate
    $g.FillPath($plateBrush, $path)
    $plateBrush.Dispose()

    $edgePen = New-Object Drawing.Pen $edge, ([Math]::Max(1.0, $size / 64.0))
    $edgePen.Alignment = [Drawing.Drawing2D.PenAlignment]::Inset
    $g.DrawPath($edgePen, $path)
    $edgePen.Dispose()

    $g.SetClip($path)
    $gloss = New-Object Drawing.Drawing2D.LinearGradientBrush `
        ([Drawing.PointF]::new($pad, $pad)), `
        ([Drawing.PointF]::new($pad, $pad + $body * 0.55)), `
        ([Drawing.Color]::FromArgb(70, 255, 255, 255)), `
        ([Drawing.Color]::FromArgb(0, 255, 255, 255))
    $g.FillRectangle($gloss, $pad, $pad, $body, $body * 0.55)
    $gloss.Dispose()

    $barH = $body * 0.07
    $barY = $pad + $body * 0.07
    $barX = $pad + $body * 0.08
    $barW = $body * 0.84
    $barPath = New-RoundPath $barX $barY $barW $barH ($barH * 0.5)
    $neon = New-Object Drawing.Drawing2D.LinearGradientBrush `
        ([Drawing.PointF]::new($barX, $barY)), `
        ([Drawing.PointF]::new($barX + $barW, $barY)), `
        $cyan, $pink
    $g.FillPath($neon, $barPath)
    $neon.Dispose()
    $barPath.Dispose()
    $g.ResetClip()

    $cell = [float]($body * 0.42 / 7.0)
    $gap = [float]($cell * 0.18)
    $letterH = 7 * $cell + 6 * $gap
    $letterW = 5 * $cell + 4 * $gap
    $oy = $pad + ($body - $letterH) * 0.58
    $mid = $pad + $body * 0.5
    $oxA = $pad + ($body * 0.5 - $letterW) * 0.5
    $oxYa = $mid + ($body * 0.5 - $letterW) * 0.5
    Stamp-BitsGlow $g $oxA $oy $bitsA $cyan $cell $gap
    Stamp-BitsGlow $g $oxYa $oy $bitsYa $pink $cell $gap

    $divPen = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(50, 255, 255, 255)), ([Math]::Max(1.0, $size / 128.0))
    $g.DrawLine($divPen, $mid, ($pad + $body * 0.28), $mid, ($pad + $body * 0.82))
    $divPen.Dispose()

    $path.Dispose()
    $g.Dispose()
    return $bmp
}

function Save-Png([Drawing.Bitmap]$bmp, [string]$path) {
    $bmp.Save($path, [Drawing.Imaging.ImageFormat]::Png)
}

$pngs = @()
foreach ($s in @(16, 20, 24)) {
    $b = New-PixelIcon $s
    $p = Join-Path $tmp ("icon-$s.png")
    Save-Png $b $p
    $b.Dispose()
    $pngs += $p
}
foreach ($s in @(32, 48, 64, 128, 256)) {
    $b = New-HiResIcon $s
    $p = Join-Path $tmp ("icon-$s.png")
    Save-Png $b $p
    $b.Dispose()
    $pngs += $p
}

Copy-Item (Join-Path $tmp 'icon-16.png') (Join-Path $outDir 'preview-16.png') -Force
Copy-Item (Join-Path $tmp 'icon-32.png') (Join-Path $outDir 'preview-32.png') -Force
Copy-Item (Join-Path $tmp 'icon-256.png') (Join-Path $outDir 'preview-256.png') -Force

$ico = Join-Path $outDir 'kb_lay.ico'
$magick = Get-Command magick -ErrorAction SilentlyContinue
if (-not $magick) { throw 'ImageMagick magick.exe not found' }

# Multi-resolution ICO; 256 is stored as PNG inside the ICO.
& magick.exe @pngs $ico
if ($LASTEXITCODE -ne 0) { throw "magick failed: $LASTEXITCODE" }

Write-Host "wrote $ico"
Get-Item $ico | Format-List FullName, Length
