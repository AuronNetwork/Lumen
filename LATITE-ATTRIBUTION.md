# Lumen: Latite attribution and implementation provenance

The original inventory below describes the **Lumen 1.4 series**, Minecraft
Bedrock 26.45 for Windows x64. The 1.5 series retains this attribution and adds
an independent [26.50 compatibility update](docs/BEDROCK-26.50.md). Updated
ownership/map/cursor layouts and tick/gamma signatures are local Lumen changes.
The [26.51 hotfix audit](docs/BEDROCK-26.51.md) verifies these retained layouts
against executable 1.26.51.1; it introduces no additional Latite code.
Reviewed on **September 14, 2026**.

**Lumen is a standalone client built using selected Latite source code and
Minecraft integration knowledge.** It does not need the Latite client running,
but its Minecraft bindings and several rendering/input techniques are derived
from Latite. It would be inaccurate to describe every part of Lumen as written
from scratch without Latite.

The upstream project is [LatiteClient/Latite](https://github.com/LatiteClient/Latite).
The source baseline used here is commit
`2271ce919a64ae4c13d208a3c4c70f083801a212`. Links below are pinned to that commit,
not to a moving branch. References to Lumen files use paths relative to the
included `Source` directory.

## What “used from Latite” means

| Category | Meaning for Lumen |
| --- | --- |
| Directly reused definitions | Exact signature strings, selected object layouts, offsets and call signatures. |
| Adapted implementation | Latite's rendering, zoom, gamma and cursor-handling techniques expressed in Lumen's smaller implementation. |
| Local implementation | Lumen's scanner, interface, launcher, configuration, validation and tests, using the adapted Minecraft bindings where needed. |
| Separate dependencies | MinHook, Geist and Windows graphics APIs have their own origins and are not Latite-authored components. |

This is a source-provenance inventory. It does not assign an ownership
percentage or claim that equivalent behavior means identical source code.

## 1. Minecraft signatures: 16 exact matches

Every one of the **16 signature strings** in `src/client.cpp::initializeHooks`
matches a string in Latite's [mc/Addresses.h][addresses]. These identify
Minecraft functions or data; they are not addresses inside Latite.dll.
Lumen's compact `signature()` scanner resolves them itself and rejects
non-unique matches. Latite's signature-store framework is not included.

| Lumen identifier | Latite signature | Use in Lumen |
| --- | --- | --- |
| `platformGlobal` | [Misc::Platform_GameCore](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L24) | Locate the platform and primary Minecraft client |
| `materialGroup` | [RenderMaterialGroup__common](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L405) | Resolve the render-material group |
| `tessBegin` | [Tessellator_begin](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L256) | Begin a batch of line vertices |
| `tessColor` | [Tessellator_color](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L263) | Set each ore marker color |
| `tessVertex` | [Tessellator_vertex](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L248) | Submit 3D vertex positions |
| `meshRender` | [MeshHelpers_renderMeshImmediately](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L270) | Draw the completed mesh immediately |
| `tickAddress` | [MultiPlayerLevel__subTick](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L117) | Run the incremental block scan during level ticks |
| `renderAddress` | [LevelRenderer_renderLevel](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L56) | Draw markers and apply projection zoom after world rendering |
| `windowAddress` | [MainWindow__windowProcCallback](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L63) | Handle local hotkeys and overlay input |
| `levelTable` | [Vtable::Level](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L48) | Find the level vtable; slot 2 supplies the leave-game hook |
| `gammaAddress` | [Options_getGamma](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L71) | Intercept the local brightness value |
| `grabCursor` | [ClientInstance_grabCursor](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L101) | Restore capture or suppress recapture while the menu is open |
| `releaseCursor` | [ClientInstance_releaseCursor](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L109) | Release gameplay mouse capture |
| `mouseGlobal` | [Misc::mouseDevice](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L29) | Read the MouseDevice input buffer |
| `mouseAddress` | [GameCore_handleMouseInput](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L140) | Filter game mouse-button events while the overlay is open |
| `releaseMouseAddress` | [AppPlatformGDK_releaseMouse](https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h#L434) | Restore the visible pointer after platform release |

Lumen also follows Latite's relative-address resolution for the platform,
material group, level vtable, render call and mouse singleton. The hook
callbacks and their lifecycle are connected directly, without Latite's
module manager or event dispatcher.

## 2. Selected Minecraft layouts and calling conventions

Lumen implements a small subset of the Minecraft SDK knowledge exposed by
Latite. It does not import the whole SDK. These values describe the source
used for the specific Bedrock build, not a stable Minecraft extension API.

| Data or operation used by Lumen | Latite reference | Selected details reproduced or adapted |
| --- | --- | --- |
| Platform to game/client lookup | [Platform_GameCore.cpp][platform], [MinecraftGame.cpp][mcgame], [ClientInstance.cpp][client] | WinMain/platform pointer chain; offsets `0x8` and `0x18`; primary-client map at `0x938`, key 0; cursor-capture state at `0x1D8`. |
| Local player and block region | [ClientInstance.cpp][client] | Virtual calls `0x1F` for the local player and `0x1E` for the block region. |
| Player position and dimension identity | [Actor.h][actor], [StateVectorComponent.h][state] | State-vector pointer at `0x218`; current position at the start of that component; dimension ownership field at `0x1C8`. |
| Reading a block | [BlockSource.h][blocksource] | Virtual slot 2 taking `BlockPos const&`; Lumen passes its compatible integer-position struct by reference. |
| Classifying a block by its name | [Block.h][block], [BlockLegacy.h][legacy], [StringUtils.h][strings] | Legacy-block pointer at `0x68`; namespaced `HashedString` at `0xE0`; its text at `0xE8`. |
| Renderer, camera origin and projection | [LevelRenderer.h][level], [LevelRendererPlayer.cpp][levelplayer] | Renderer-player pointer `0x468`; origin `0x660`; projection fields `0xF58` and `0xF6C`. |
| Vertex and color submission | [ScreenContext.h][screen], [Tessellator.cpp][tess], [Tessellator.h][primitive] | Shader-color pointer `0x30`, tessellator pointer `0xB8`, line-list primitive and the begin/color/vertex call signatures. |
| Immediate mesh drawing | [MeshUtils.cpp][mesh] | Immediate-render function signature and its `0x58`-byte auxiliary buffer. |
| Material names and creation | [MaterialPtr.cpp][material], [StringUtils.h][strings], [Crypto.h][crypto] | `ui_fill_color` and `selection_box`, material-group virtual slot 1, shared-pointer output parameter, HashedString layout and 64-bit multiply/XOR hashing. |
| Mouse event storage | [MouseDevice.h][mouse], [MouseDevice.cpp][mousecpp], [MouseAction.h][action] | Seven button states and input-vector layout; event coordinates, deltas, action/state bytes, pointer ID and motionless flag. Lumen checks the 20-byte event layout at compile time. |

The small `Pos`, `Vec3` and `Color` types in Lumen are local representations
compatible with the selected calls. They do not incorporate Latite's full
math, entity or rendering class libraries.

## 3. Xray marker rendering

The basic 3D drawing sequence comes from Latite's
[Graphics3DScriptingObject::onRenderLevel][graphics3d], its
[3D drawing utilities][draw3d], and the SDK helpers above:

- Obtain the world renderer and screen context.
- Set a usable shader color and begin line geometry.
- Subtract the camera origin from world-space vertex positions.
- Assign colors and submit vertices to Minecraft's tessellator.
- Render with the selected material through the immediate mesh helper.

Lumen adapts that sequence into `createMaterials()` and `drawUnsafe()` in
`src/client.cpp`. It batches the twelve edges of each ore box, preserves
the prior shader color and keeps its material ownership references alive.
Its through-wall toggle selects between the two material handles locally.
This is an adaptation of the rendering building blocks, not a direct copy
of Latite's JavaScript command queue or its complete drawing API.

The **ore scanner itself** in `src/scanner.h` was implemented for the
Xray Light/Lumen work: ore-name classification, filters, radius traversal,
per-tick query budget, cached results, refresh/revalidation, closest-marker
selection and resets. It depends on Latite-derived block access and drawing
bindings, but it is not a transplanted Latite Xray module.

## 4. Zoom and Fullbright

**Zoom:** Latite's [Zoom::onRenderLevel][zoom] multiplies the camera's X/Y
projection fields. Lumen uses that technique and the same field locations,
with a render hook that follows the post-render timing shown in
[LevelRendererHooks.cpp][levelhooks]. Lumen adds its own hold-C input,
2×–20× setting, projection validation and tracking/restoration of its last
write in `src/client.cpp` and `src/view_features.h`.

Latite's complete Zoom module is not included. Its animation, cinematic
camera, mouse-wheel adjustment, sensitivity adjustment and hide-hand
options are not ported into Lumen.

**Fullbright:** The hook target and original-call structure come from
[OptionHooks.cpp][options]; the brightness override comes from
[Fullbright.cpp][fullbright] and the default value **25** from
[Fullbright.h][fullbrighth]. Lumen applies that value when enabled and
returns the game's original gamma value when disabled. The B hotkey,
overlay control and saved option are integrated through Lumen's own code.

## 5. Mouse capture, hotkeys and menu lifecycle

Lumen adapts the following from [GeneralHooks.cpp][general],
[ClientInstance.cpp][client] and [ScreenManager.cpp][screens]:

- The window-procedure hook used to receive keyboard input.
- Releasing the client cursor when a custom screen opens and grabbing it
  again when that screen closes.
- Suppressing `ClientInstance::grabCursor` while the custom screen is open.
- Setting a visible arrow after the platform releases mouse capture.
- Calling the original GameCore mouse handler before filtering its queued
  button events.
- Releasing capture again if gameplay attempts to retain it during the menu.
- The level-leave hook obtained from the level vtable's slot 2.

Lumen supplies the surrounding implementation: Insert/Escape behavior,
focus handling, control callbacks, held-key cleanup, the fence that consumes
a closing mouse click until release, and movement-preserving button
filtering. Its pointer hit-testing uses Windows cursor coordinates and its
own overlay transform. Latite's ScreenManager, ClickGUI, event classes and
plugin input callbacks are not compiled into Lumen.

## 6. DirectX in-game overlay

`src/overlay.cpp` adapts the integration approach in
[DXHooks.cpp][dx] and [Renderer.cpp][renderer]:

- Create temporary graphics objects to locate the relevant vtable entries.
- Hook Present, ResizeBuffers, ResizeBuffers1 and ExecuteCommandLists.
- Obtain a compatible graphics device and direct command queue.
- Use Direct2D to draw onto a swap-chain surface.
- For DirectX 12, use D3D11On12 wrapped resources, acquire/release them around
  drawing and flush the D3D11 context.
- Release and recreate drawing resources when the swap chain is resized.

The corresponding vtable indices in Lumen are 8, 13, 39 and 10. These are
graphics-interface slots, separate from the 16 Minecraft byte signatures.
The D3D11On12 technique is also described in the
[Microsoft Direct2D/D3D11On12 reference](https://learn.microsoft.com/en-us/windows/win32/direct3d12/d2d-using-d3d11on12).

The Lumen panel layout, Auron styling, toggles, sliders, ore chips, text,
hover feedback, scaling and local Geist-font loading were implemented for
Lumen. This is not Latite's renderer/UI subsystem copied wholesale; for
example, Lumen does not port Latite's VSync/tearing controls or blur effects.

## 7. Local fixes in the Latite working copy

The reviewed Latite checkout is based on the pinned commit but contains
earlier local patches. They must not be credited as unchanged upstream code.

| Local Latite file | Local change | Relationship to Lumen |
| --- | --- | --- |
| [GameScriptingObject.cpp][gamejs] | Changed the block query to the `BlockPos const&` overload; added a 26.45 guard and a scripting capability marker. | Lumen retains the corrected by-reference call. The overload was already declared upstream; selecting it to repair our reader was a local fix. Lumen does not include the Chakra capability marker or scripting object. |
| [MaterialPtr.cpp][material] | Retained the owning shared pointers instead of returning a raw pointer whose local owner immediately expired. | Lumen applies this ownership correction through its two persistent shared-pointer material handles. The underlying creation API comes from Latite. |
| [Latite.h][latiteh] | Added a local version suffix and narrowed the supported-version string for the repaired Latite build. | Historical repair context. Lumen implements its own file-version check and does not include this Latite header. |
| [DrawUtil3D.h][drawh] | Line-ending changes only in the reviewed working copy. | No separate behavioral repair is attributed to this diff. |

The links in this table show the original upstream files. The local fixes
are identified by comparison with Git HEAD, not falsely presented as changes
already contained in that upstream commit.

**Historical permission check:** Before Lumen 1.2.3, the world reader also
used Latite's [Actor::getCommandPermissionLevel][actorcpp] call at virtual
slot `0x66`. The local scan/render gate was removed in 1.2.3, along with
that accessor call and the OP-required message. It is therefore historical
Latite-derived integration, not part of the current runtime path.

## 8. Lumen-specific work and excluded systems

The following application work was written for Lumen/Xray Light:

- The native launcher, process/module checks and DLL-loading workflow in
  `src/launcher.cpp`; it does not embed the separate Latite Launcher project.
- The native Auron starter UI in `src/launcher_ui.cpp` and `launcher_ui.h`,
  designed in Figma for Lumen 1.3.0. It uses the same local Geist font loading
  approach as the Lumen overlay and standard Win32/Direct2D/DirectWrite APIs;
  no additional Latite source was introduced for this launcher redesign.
- The incremental ore scanner, marker batching and configurable ore filters.
- The compact signature scanner, exact-build guard, failure handling,
  diagnostic log and configuration migration/persistence.
- The Auron overlay, its interaction state, English localization and controls.
- Zoom restoration bookkeeping, mouse-click release fencing and the
  scanner/view/mouse regression tests.
- The standalone CMake build and release packaging.

These are local implementations **around the credited Latite-derived
bindings and techniques**, not a claim that their entire foundation is original.

Lumen does not include Latite's client core, module manager, plugin manager,
ChakraCore JavaScript runtime, scripting API, command/chat handling, event
dispatcher, updater, full UI asset set or its other feature modules.
The current build compiles `client.cpp` and `overlay.cpp` into Lumen.dll,
uses local headers, and links MinHook and Windows libraries. No Latite.dll
is linked or required at runtime.

## 9. Other dependencies and license attribution

| Component | Origin and treatment |
| --- | --- |
| Latite-derived source and definitions | Credit belongs to the Latite project and its contributors. The upstream repository includes [GNU GPL version 3][license]; Lumen source declares `GPL-3.0-only` and ships its corresponding source and license. |
| MinHook | Separate library by Tsuda Kageyu, statically linked. Its bundled license also preserves notices for the included disassembler code, including Hacker Disassembler Engine by Vyacheslav Patkov. See `third_party/minhook/LICENSE.txt` in Source. |
| Geist | Font family by Vercel, loaded from `assets`; original SIL Open Font License text is included as `assets/Geist-OFL.txt`. |
| Windows graphics/platform APIs | Direct3D, DXGI, Direct2D, DirectWrite and Win32 APIs provided by Microsoft. They are platform dependencies rather than Latite-owned code. |
| Auron appearance | Colors from the existing Auron website stylesheet; layout and Figma design produced for this Lumen project. |

The package does not include Minecraft executables or game assets. This
document records attribution and the licenses shipped with the project;
it does not replace any of those license texts.

## 10. Review method and scope

This inventory was checked against Lumen 1.2.3 source, its CMake build,
the local Latite commit and its working-copy diff. All 16 Minecraft signature
strings were matched exactly to `mc/Addresses.h`. Referenced upstream paths
were checked against the pinned Git commit. Selected layouts and feature
implementations were read and compared directly.

The original provenance audit used the 1.2.3 source baseline. For the 1.3.0
launcher redesign, the client source was compared with that baseline: only
the startup version string changed; overlay and gameplay headers are identical.
The new launcher UI and its updated status plumbing are local work. The
signature/ABI inventory above therefore remains applicable. This comparison
does not add a Minecraft runtime test. See TESTING.md for the current
launcher UI acceptance limit and inherited non-operator test limit.

The 1.4 series adds the local GitHub updater, installer and release workflow.
Minecraft bindings remain the audited baseline. Generated version metadata
replaces fixed strings. nlohmann/json is a separate MIT-licensed dependency
used by the updater, not Latite code. Release hashes are provided in each
release's SHA256SUMS.txt; the complete source is in Lumen-Source.zip.

## Short credit statement

> Lumen uses and adapts Minecraft integration definitions, signatures, 3D
> rendering techniques, Zoom/Fullbright behavior and mouse/DirectX integration
> from the Latite Client project and its contributors. Lumen runs independently
> and adds its own scanner, launcher, configuration and Auron interface.
> Latite-derived work is credited under the included GPL-3.0-only release;
> MinHook and Geist retain their separate license notices.

[addresses]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/Addresses.h
[platform]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/client/game/Platform_GameCore.cpp
[mcgame]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/client/game/MinecraftGame.cpp
[client]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/client/game/ClientInstance.cpp
[actor]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/world/actor/Actor.h
[actorcpp]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/world/actor/Actor.cpp
[state]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/entity/component/StateVectorComponent.h
[blocksource]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/world/level/BlockSource.h
[block]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/world/level/block/Block.h
[legacy]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/world/level/block/BlockLegacy.h
[strings]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/deps/core/StringUtils.h
[crypto]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/util/Crypto.h
[level]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/client/renderer/game/LevelRenderer.h
[levelplayer]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/client/renderer/game/LevelRendererPlayer.cpp
[screen]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/client/renderer/screen/ScreenContext.h
[tess]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/client/renderer/Tessellator.cpp
[primitive]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/client/renderer/Tessellator.h
[mesh]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/client/renderer/MeshUtils.cpp
[material]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/client/renderer/MaterialPtr.cpp
[graphics3d]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/client/script/globals/Graphics3DScriptingObject.cpp
[draw3d]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/util/DrawUtil3D.cpp
[levelhooks]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/client/memory/hook/hooks/LevelRendererHooks.cpp
[zoom]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/client/feature/module/modules/game/Zoom.cpp
[fullbright]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/client/feature/module/modules/visual/Fullbright.cpp
[fullbrighth]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/client/feature/module/modules/visual/Fullbright.h
[options]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/client/memory/hook/hooks/OptionHooks.cpp
[mouse]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/client/game/MouseDevice.h
[mousecpp]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/client/game/MouseDevice.cpp
[action]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/mc/common/client/game/MouseAction.h
[general]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/client/memory/hook/hooks/GeneralHooks.cpp
[screens]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/client/screen/ScreenManager.cpp
[dx]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/client/memory/hook/hooks/DXHooks.cpp
[renderer]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/client/render/Renderer.cpp
[gamejs]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/client/script/globals/GameScriptingObject.cpp
[latiteh]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/client/Latite.h
[drawh]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/src/util/DrawUtil3D.h
[license]: https://github.com/LatiteClient/Latite/blob/2271ce919a64ae4c13d208a3c4c70f083801a212/LICENSE
