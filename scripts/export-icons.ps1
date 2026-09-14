# Mechanical format/size exports from the finished Lumen artwork. No design changes.
param(
    [string]$Source = (Join-Path $PSScriptRoot '../assets/lumen-icon.png'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../assets')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$sourceImage = [System.Drawing.Image]::FromFile((Resolve-Path -LiteralPath $Source).Path)
$null = New-Item -ItemType Directory -Force -Path $OutputDirectory
$iconSizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)
$frames = @{}
try {
    foreach ($size in ($iconSizes + @(180, 512))) {
        $bitmap = New-Object System.Drawing.Bitmap($size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb))
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.DrawImage($sourceImage, (New-Object System.Drawing.Rectangle(0, 0, $size, $size)))
            $stream = New-Object System.IO.MemoryStream
            try {
                $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
                $frames[$size] = $stream.ToArray()
            } finally { $stream.Dispose() }
            if ($size -in @(32, 180, 512)) {
                [System.IO.File]::WriteAllBytes((Join-Path $OutputDirectory "lumen-$size.png"), $frames[$size])
            }
        } finally { $graphics.Dispose(); $bitmap.Dispose() }
    }
    # Windows Vista+ ICO supports lossless RGBA PNG frames, including 256px.
    $file = [System.IO.File]::Create((Join-Path $OutputDirectory 'lumen.ico'))
    $writer = New-Object System.IO.BinaryWriter($file)
    try {
        $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$iconSizes.Count)
        $offset = 6 + 16 * $iconSizes.Count
        foreach ($size in $iconSizes) {
            $dimension = if ($size -eq 256) { 0 } else { $size }
            $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
            $writer.Write([byte]0); $writer.Write([byte]0)
            $writer.Write([uint16]1); $writer.Write([uint16]32)
            $writer.Write([uint32]$frames[$size].Length); $writer.Write([uint32]$offset)
            $offset += $frames[$size].Length
        }
        foreach ($size in $iconSizes) { $writer.Write([byte[]]$frames[$size]) }
    } finally { $writer.Dispose(); $file.Dispose() }
} finally { $sourceImage.Dispose() }
Write-Output "Exported Lumen ICO (16-256px) and PNG (32, 180, 512px) to $OutputDirectory"
