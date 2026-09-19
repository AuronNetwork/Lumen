# Attribution and licenses

Lumen 1.6 series, a local extension of Xray Light, September 19, 2026.
License: GPL-3.0-only. Version 1.2.2 translated the interface into English.
Version 1.2.3 removes Lumen's local operator requirement for scanning and
rendering ore markers. Version 1.3.0 adds an independently implemented
native launcher interface designed in Figma for Lumen.
Version 1.5.0 independently updates game ownership, client-map layout, cursor
capture and tick/gamma signatures for Bedrock 26.50; see docs/BEDROCK-26.50.md.
Version 1.5.1 targets the independently audited Bedrock 26.51 hotfix, retaining
those integration layouts; see docs/BEDROCK-26.51.md for validation limits.
Version 1.6.0 extends the local block classifier and UI with opt-in chest,
trapped-chest and ender-chest filters. It adds no further Latite code or game hooks.

See [LATITE-ATTRIBUTION.md](LATITE-ATTRIBUTION.md) for the detailed source
inventory, exact signature mapping, local corrections, dependency boundaries
and a reusable credit statement.

This small native client uses or adapts Minecraft ABI descriptions,
signatures and the basic 3D rendering approach from the public Latite project:
https://github.com/LatiteClient/Latite
Source commit: 2271ce919a64ae4c13d208a3c4c70f083801a212.

Relevant foundations: mc/Addresses.h, ClientInstance, Platform_GameCore,
MinecraftGame, Actor/StateVectorComponent, Block/BlockLegacy, HashedString,
LevelRenderer/LevelRendererPlayer, ScreenContext, Tessellator, MeshUtils and
Graphics3DScriptingObject::onRenderLevel. The BlockPos calling convention fix
for 26.45 and retained material ownership references are local corrections.
The original Latite authors retain their rights.

Zoom adapts projection scaling from Zoom::onRenderLevel and the FovX/FovY
fields of LevelRendererPlayer. Fullbright adapts Options_getGamma and
Fullbright's gamma value of 25. Lumen adds its own key handling, saved
settings, projection restoration and validation.

Latite does not run alongside this package. The launcher, scanner, version
check, settings interface and lifecycle are implemented independently.
There is no plugin host or JavaScript engine.

MinHook is included as source under third_party/minhook and linked
statically. Its license and author notices are preserved in
third_party/minhook/LICENSE.txt and the source files.

The package contains no Minecraft executables or game assets. Complete
corresponding source is available in Lumen-Source.zip alongside each release.

The in-game interface also adapts the DirectX/Direct2D approach from Latite's
DXHooks/Renderer, ClientInstance::grabCursor/releaseCursor,
GameCore::handleMouseInput, MouseDevice and the cursor hooks in GenericHooks.
The panel and its control bindings are implemented specifically for Lumen.
Microsoft reference:
https://learn.microsoft.com/en-us/windows/win32/direct3d12/d2d-using-d3d11on12

Font: Geist by Vercel, SIL Open Font License 1.1.
Source: https://github.com/vercel/geist-font
The original font license is included in assets/Geist-OFL.txt.
Auron colors come from the existing Auron website stylesheet.
Original Figma design:
https://www.figma.com/design/Lfm0C6WFqEGv6z6XLKszSW?node-id=3-2

Launcher Figma design:
https://www.figma.com/design/Lfm0C6WFqEGv6z6XLKszSW?node-id=11-17

The 1.4 series adds an independently implemented GitHub updater and Inno Setup
installer. No additional Latite code is introduced by these components.

nlohmann/json 3.12.0 by Niels Lohmann, MIT license. The complete notice is in
third_party/nlohmann/LICENSE.MIT (source) and JSON-LICENSE.txt (installed copy).
Inno Setup is a build/installer tool by Jordan Russell and Martijn Laan:
https://jrsoftware.org/isinfo.php
