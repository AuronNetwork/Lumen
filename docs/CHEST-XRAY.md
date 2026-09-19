# Chest Xray (1.6.0 candidate)

Target: Minecraft Bedrock 26.51 for Windows x64, executable 1.26.51.1.

## Behavior

Insert opens the menu. Enable Xray with its switch or X, then select any of the
three new **Chest Filters** below the existing ore filters:

| Filter | Block identifier | Outline color |
| --- | --- | --- |
| Chests | minecraft:chest | Amber |
| Trapped Chests | minecraft:trapped_chest | Coral |
| Ender Chests | minecraft:ender_chest | Lavender |

These identifiers are listed in Microsoft's [Bedrock block reference](https://github.com/MicrosoftDocs/minecraft-creator/blob/main/creator/Commands/enums/Block.md).
The existing BlockSource reader resolves them through the same BlockLegacy name
field used for ores. No new native calls, signatures or offsets are required.

Markers share Xray's 4-24 block radius, nearest-first 32-512 marker limit,
bounded scanning budget and Show through walls switch. Two adjacent chest blocks
are separate markers, so a double chest consumes two marker slots. Removed or
replaced chests are cleared on a subsequent refresh. World changes clear the
shared scanner as before.

Each chest type can be selected independently or together with ores. The original
ten ore-filter bits and defaults are unchanged; the three new bits start off.
All selections use the existing Xray/Ores settings key, now retaining 13 valid
bits. Unsupported reader IDs are discarded before bit shifting or color lookup.

This feature displays block locations only. It does not inspect inventories.
Barrels, shulker boxes, copper chests, chest boats/minecarts and custom blocks
are outside this version's three supported identifiers. Like ore Xray, it can
only mark blocks already available to the client.

## Automated and manual validation

The scanner suite checks exact identifiers and namespace rejection, original
ore defaults, independent and combined filters, adjacent chest positions,
radius/marker/read limits, removal/replacement and invalid positive IDs. The
existing ore, world-reset, zoom, mouse and updater tests remain in place.

The 1.6.0 native x64 build and all six CTest cases pass. The production overlay
drawing code was rendered offscreen with sample data at 1280x720 and 1280x900;
the new filter labels and status fit without clipping or overlapping controls.
This checks layout only, not live cursor interaction or game rendering.

New in-game chest behavior has not yet received user acceptance. Test after
saving and fully restarting Minecraft:

1. In a local test world, place a normal chest, a double chest, a trapped chest
   and an ender chest within the selected radius, with an opaque wall between
   the player and the chests.
2. Enable Xray and Show through walls. Toggle each chest filter separately,
   then together with an ore filter; confirm the matching colored markers.
3. Remove one double-chest half and confirm only that marker disappears after
   refresh. Move outside the radius or leave/re-enter the world to check cleanup.
4. Disable the chest filters and confirm ore Xray, C zoom, B brightness and
   Insert/Escape/mouse controls still work.
5. Restart Minecraft/Lumen and confirm the ore and chest selections were retained.

The 1.5.7 release's accepted gameplay remains the baseline. It does not establish
acceptance of these new chest markers. The candidate is kept local pending this
test and is not automatically published to existing users.
