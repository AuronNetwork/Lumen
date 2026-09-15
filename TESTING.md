# Validation and acceptance limits

The 1.5.0 candidate updates the integration for Minecraft Bedrock 26.50,
Windows package 1.26.5004.0. All 16 final signatures are unique in the installed
running module, and the changed ownership/map/cursor layouts were checked with
read-only inspection. The build and all six CTest cases pass. A clean-process
gameplay test is pending; see [the audit](docs/BEDROCK-26.50.md).

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
