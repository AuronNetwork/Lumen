# Lumen icon

The Lumen mark combines the Auron Network triangle and rising arrow with a yellow light beam. The wordmark is omitted so the symbol remains readable at small sizes.

- `lumen-icon.png`: generated master artwork, with alpha transparency.
- `lumen.ico`: Windows application icon and browser favicon; contains 16, 20, 24, 32, 40, 48, 64, 128 and 256 pixel frames.
- `lumen-32.png`: browser favicon.
- `lumen-180.png`: touch icon / README preview.
- `lumen-512.png`: large application or website icon.

The launcher embeds the ICO as resource 101 and assigns it to its window class. Explorer, the taskbar and launcher shortcuts use this icon. Inno Setup embeds the same ICO; the installed application's uninstall entry refers to Lumen.exe.

Regenerate the format exports on Windows with `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/export-icons.ps1`. This resizes and encodes the finished artwork without changing the design. Generated exports are committed, so building Lumen does not require an image service.

For a website, serve `lumen.ico` as `/favicon.ico` or link to it explicitly:

```html
<link rel="icon" href="/assets/lumen.ico" sizes="any">
<link rel="icon" type="image/png" sizes="32x32" href="/assets/lumen-32.png">
<link rel="apple-touch-icon" sizes="180x180" href="/assets/lumen-180.png">
```

## Artwork provenance

Created with the built-in Imagegen tool from the existing Auron Network icon (`auron-network-icon.png`) supplied by the local Auron Trainer project. This artwork is separate from Latite's branding. The original Auron logo was left unchanged.

Generation prompt:

> Use case: logo-brand. Edit the supplied Auron Network logo into a distinctive Lumen Windows application icon. Preserve the recognizable upright triangular A outline and the central curved, ascending swoosh/arrow silhouette from the reference. Remove the AURON wordmark entirely and remove the yellow square perimeter border. Recenter and enlarge only the symbol. Make the triangular outline warm white (#f5f3ea), generously thick for excellent 16-pixel readability. Make the central ascending curved arrow a bold signal-yellow (#ffce00) light beam with one tiny white highlight at its pointed upper-right tip, suggesting illumination / lumen, integrated into the arrow rather than a separate decorative object. Background: a single very dark almost-black (#080808) rounded-square app tile, moderate rounded corners, with true transparent alpha outside the tile. Tile occupies 96 percent of a square 1024x1024 canvas; centered mark has 14 percent safe margins. Professional flat, crisp vector-like forms, strong contrast, carefully balanced negative space, straight triangle edges and elegant controlled swoosh curve, monochrome yellow/ivory/black palette only. No text or letters beyond the original abstract A shape, no mockup, no labels, no watermarks, no texture, no 3D bevels, no drop shadow, no blur, no sprawling glow. Output one finished square icon, not a presentation sheet. Reference is the actual Auron brand icon and is the edit target.
