# Validation and acceptance limits

## Chest Xray acceptance (1.6 series)

The 1.6.0 candidate adds independent chest, trapped-chest and ender-chest filters to
the existing Bedrock 26.51 scanner and overlay. No game hooks, offsets or version
gate changed. Automated scanner coverage includes mixed ore/chest selection,
adjacent double-chest halves, shared limits, removal/replacement and invalid IDs.
The native x64 build and all six CTest cases pass. Offscreen renders of the
production menu at 1280x720 and 1280x900 confirm the filter labels fit without
clipping or overlapping controls; these use sample state and do not test input.
On September 19, 2026, the user reported that the requested chest test passed:
normal, trapped and ender chests marked behind walls, removed chests disappearing,
and filter selections retained after restart. This is user-reported acceptance
of the 1.6.0 candidate, not an independently observed gameplay test. Public 1.6
releases retain that candidate's feature code, with version metadata supplied by
CI. See [the checklist and limits](docs/CHEST-XRAY.md).

## Bedrock 26.51 baseline (1.5 series)

The 1.5.1 candidate targets Minecraft Bedrock 26.51, Windows package
1.26.5101.0 (file version 1.26.51.1). All 16 signatures are unique in the
installed running module. Read-only inspection confirmed the ownership, map,
actor, block, camera and gamma layouts from the 26.50 candidate. Sampled native
instruction sequences match after masking relative addresses. The build and all
six CTest cases pass; see [the 26.51 audit](docs/BEDROCK-26.51.md). The earlier
26.50 candidate did not receive gameplay acceptance before this hotfix arrived.

On September 16, 2026, the user confirmed that everything works after receiving
the 1.5.1 candidate and the requested clean-restart/local-world checklist:
Xray and ore filters, C hold/release, B on/off, Insert/Escape with usable mouse,
and leaving/re-entering the world. This records user-reported gameplay
acceptance, not an independently observed visual test. The release retains the
candidate's gameplay code; CI supplies the public release version. This report
does not establish a separate non-operator or end-to-end automatic-update test.

The latest local runtime log independently records file version 1.26.51.1,
READY Lumen 1.5.1, successful ore-box rendering, zoom rendering, gamma override,
and menu open/close with blocked game cursor grabs. No error was logged in that
startup segment. These records corroborate feature execution, not visual quality.

The release workflow builds the native x64 programs, runs the scanner,
view/projection/gamma, mouse, update-policy, SHA-256 and exact Bedrock-version
tests, and validates
the installer/source ZIPs. It also installs into an isolated runner folder,
verifies installed files and exercises uninstallation.

These checks cover the pipeline and selected logic, not Minecraft gameplay.
The user confirmed Xray, Zoom, Fullbright and the cursor fix in 1.2.1. The no-OP
behavior introduced in 1.2.3 has not yet been accepted in a non-operator session.
The 1.3.0 native starter's initial UI inspection stopped before acceptance.
In the 1.4.0 local build, the Ready screen rendered correctly, Tab showed the
expected focus outline, and Close launcher exited the test window. No injection
was performed in that UI check. DPI transitions and other UI states remain
outside that bounded visual check.

The 1.4 series adds GitHub updates, installer packaging and generated version
metadata. Minecraft ABI and feature behavior are retained. Each release
identifies the exact source commit it builds.

Before claiming support for another Minecraft build, test Insert/Escape, mouse
capture, all features, zoom restoration, ore filters, world exit/re-entry and
a non-operator world. A compile cannot validate runtime offsets or server data.

For updater acceptance, check current/newer version cases, Minecraft-running
deferral, offline fallback, checksum rejection, parent-process handoff,
locked-file refusal, successful restart and preserved settings. Record automated
checks and manual observations separately. The installer is not signed.
