# Minecraft Bedrock 26.51 compatibility audit

Target: Windows x64 package **1.26.5101.0**, executable file version
**1.26.51.1**. Inspected on September 16, 2026. This hotfix follows the
[26.50 integration update](BEDROCK-26.50.md). The user accepted the Lumen 1.5.1
local candidate in gameplay on September 16, 2026. Public 1.5 releases retain
that gameplay code and receive their release version from CI.

## Verified compatibility

Read-only inspection of a fresh dump of the installed running module established:

- The loaded PE version resource reports exactly 1.26.51.1, matching the
  installed package and the previous DLL's version-rejection log.
- All **16** existing signatures match exactly once in the executable section.
  Their resolved addresses changed where the game moved code; Lumen resolves
  them at runtime and contains no absolute process addresses.
- Main window +8 -> platform +0x20 -> application +0x48 -> game remains valid.
  The client map at game +0x970 has valid sentinel and node flags, key 0,
  the concrete client at node +0x30 and a matching client +0x1A8 game owner.
- Native capture/release methods retain game +0x1E8. Client virtual slots
  0x1E/0x1F retain the BlockSource/LocalPlayer getters. Their WeakEntity and
  Actor resolvers retain the inspected instruction sequences.
- Resolving the local actor read-only confirms its +0x218 state-vector and
  +0x1C8 dimension fields. The dimension getter still uses +0xF0 for BlockSource;
  BlockSource slot 2 still takes BlockPos by reference. A native default air
  block resolves through Block +0x68 and BlockLegacy +0xE8 to `minecraft:air`.
- Client +0x1C0 -> LevelRenderer +0x468 still resolves the render player.
  Camera origin +0x660 and projection scales +0xF58/+0xF6C are readable and
  consistent with the actor position and a normal projection.
- The native gamma getter retains option identifier **0x32**. The option-name
  table independently places GAMMA at index 50.

Twenty-one sampled native code regions (32-512 bytes each) match the 26.50
instruction sequences after masking RIP-relative displacements and branch/call
destinations. This includes the twelve signature-resolved function entries,
game update/cursor methods, client/entity getters, dimension getter and block
lookup. This is a bounded structural comparison, not proof of unchanged
whole-function behavior or visual feature acceptance.

No signature or field-offset changes were needed beyond the previous 26.50
integration. The exact version gate, tests, launcher/menu labels, installer
instructions and release documentation now target Bedrock 26.51. Other executable
revisions remain rejected before hook installation.

Inspection used only PROCESS_QUERY_INFORMATION and PROCESS_VM_READ. It did not
patch or invoke game code, close Minecraft or change WindowsApps permissions.
Local game-code dumps stay outside the repository and release/source packages.

## Acceptance status

The native DLL and launcher compile with MSVC x64. All six CTest cases pass,
including exact version checks that accept 1.26.51.1 and reject the previous
builds, other revisions and package-version components passed as file versions.
The installer and matching source ZIP have been built and archive-checked.

The 26.50 candidate was not accepted in gameplay before this hotfix arrived.
For the 1.5.1 candidate, the user was asked to fully restart Minecraft and test
in a local world: Xray and ore filters, C hold/release, B on/off, Insert/Escape
with usable mouse, and leaving/re-entering the world. On September 16, 2026,
the user confirmed that everything works. This is user-reported acceptance of
that checklist, separate from the read-only audit and automated checks above.
It is not an independently observed visual test or a separate confirmation of
non-operator behavior or the entire automatic-update handoff.

The local runtime log from the fresh 16:40:21 startup reports Minecraft
1.26.51.1 and READY Lumen 1.5.1, followed by RENDER_OK (ore boxes), ZOOM_RENDER_OK,
FULLBRIGHT_GAMMA_OK and menu open/close events with blocked game cursor grabs.
There is no error entry in that startup segment through 16:41:18. These records
confirm the feature paths executed; the visual result is supported by the
user's report.

The public release retains the accepted candidate's gameplay code. CI rebuilds
it with the release version and runs the automated tests and isolated installer
lifecycle checks. Existing settings are preserved. Fully restart Minecraft
before loading an update: an old DLL remains loaded even after version rejection.
