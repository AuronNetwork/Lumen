# Minecraft Bedrock 26.51 compatibility audit

Target: Windows x64 package **1.26.5101.0**, executable file version
**1.26.51.1**. Inspected on September 16, 2026. This hotfix follows the
[26.50 integration update](BEDROCK-26.50.md). Lumen 1.5.1 is an unpublished
local candidate pending gameplay acceptance.

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
Neither that candidate nor this update has been published to the stable updater.
A clean-process local-world test is still required:

1. Save and fully close Minecraft, install this candidate, then start it through
   Lumen. The old DLL remains loaded even after its version check fails.
2. Check Xray, ore filters and drawing while moving through the world.
3. Hold/release C and toggle B on/off; verify normal view restoration.
4. Open/close Insert and Escape; check cursor, sliders and input capture without
   opening Minecraft's pause menu or causing unintended block interaction.
5. Leave/re-enter the world and confirm normal operation. Separately verify
   non-operator behavior before claiming its acceptance.

Existing settings are preserved. A successful compile, signature scan or memory
read does not replace these gameplay checks.
