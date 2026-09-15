# Minecraft Bedrock 26.50 compatibility audit

Target: Windows x64 package **1.26.5004.0**, executable file version
**1.26.50.4**. This is an independent Lumen update of the original Latite-based
26.45 integration. The Latite reference remains commit
`2271ce919a64ae4c13d208a3c4c70f083801a212`.

## Verified changes

Read-only inspection of the installed game's running module established:

| Area | Previous integration | 26.50 integration and evidence |
| --- | --- | --- |
| Game ownership chain | Main window +8 → platform +0x18 → game | Main window +8 → platform +0x20 → application +0x48 → game. The resulting object's virtual update method calls MinecraftGame::_update. The application accessor also reads/writes its +0x48 game field. |
| Client map | game +0x938, value interpreted as shared_ptr | game +0x970. Node key remains +0x20; the concrete ClientInstance pointer is at node +0x30, after an interface pointer. Native game map consumers load +0x30 before calling ClientInstance virtual methods. |
| Client owner | implicit | ClientInstance +0x1A8 must point back to the same game. Invalid map headers or mismatched owners stop world access. |
| Captured cursor | game +0x1D8 | game +0x1E8, verified in the native grab/release methods. |
| Level tick | short prologue matched one function | The old prefix matches two functions. Extending it through the saved XMM register offsets identifies exactly one Level tick function. |
| Gamma | option identifier 0x35 | 0x32, verified against the ordered option-name table (GAMMA at index 50), the float getter and its virtual tables. |
| Version gate | any executable 1.26.45.* | exactly file version 1.26.50.4; other revisions are rejected before installing hooks. |

All **16** final signatures matched exactly once in the executable section of
the running 26.50 module. No ASLR-dependent absolute address is stored in Lumen.

The following existing layouts were checked and retained:

- ClientInstance virtual slots 0x1E/0x1F resolve BlockSource/LocalPlayer.
- Actor +0x218 contains the state-vector pointer; live position values were
  consistent with the game view. Actor +0x1C8 contains the dimension pointer.
- The native dimension BlockSource getter and BlockSource virtual slot 2 retain
  the BlockPos-by-reference block lookup.
- A native default air block resolved through Block +0x68 and BlockLegacy +0xE8
  to `minecraft:air`.
- LevelRenderer +0x468 contains the render player. Camera origin +0x660 and
  projection scales +0xF58/+0xF6C were readable and plausible. Native render code
  still reads the same render-player field.

Inspection used PROCESS_QUERY_INFORMATION / PROCESS_VM_READ. It did not patch
the process, call remote game functions, alter WindowsApps permissions or close
Minecraft. Local audit dumps contain game code and are intentionally excluded
from the source and release packages.

## Acceptance status

The updated native DLL and launcher compile with MSVC x64. All six CTest cases
pass, including a new exact-version test that rejects the previous build,
unverified revisions and accidentally passing package-version components in
place of executable-version components.

This establishes signature/layout evidence and automated logic checks. It does
not establish visual feature acceptance. A clean Minecraft restart and a local
world test of Xray, ore filters, C hold/release, B on/off, Insert/Escape, mouse
capture and world exit/re-entry are still required before public rollout.

The old DLL remains loaded after its version check rejects a game build. Do not
attempt to unload it or inject over it; restart Minecraft before testing this
candidate. Existing settings are preserved.
