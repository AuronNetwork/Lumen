<img src="assets/lumen-180.png" width="80" height="80" alt="Lumen — Auron triangle with a yellow light beam">

# Lumen

A lightweight Windows client for **Minecraft Bedrock 26.45** with Xray, Zoom,
Fullbright and an Auron-styled in-game menu. Lumen runs independently of Latite.

[Download the installer ZIP](https://github.com/AuronNetwork/Lumen/releases/latest/download/Lumen-Installer.zip)
 · [Releases and source](https://github.com/AuronNetwork/Lumen/releases)

## Install

1. Download and extract **Lumen-Installer.zip**.
2. Run **Lumen-Setup.exe**. Installation is per user; no administrator rights
   are required. The default location is `%LOCALAPPDATA%\Programs\Lumen`.
3. Save and restart Minecraft if Lumen, Xray Light or Latite is already loaded.
4. Open **Lumen** from the Start menu and select **Launch Lumen**.
5. Enter a world and press **Insert**.

The installer includes Lumen.exe, Lumen.dll, the required Geist fonts, licenses
and a source-download link. It provides an uninstaller; uninstalling preserves
your Lumen settings. The installer is not Authenticode-signed, so Windows may
show an unknown-publisher or SmartScreen warning. SHA-256 hashes are included
with each release.

## Controls

| Key | Action |
| --- | --- |
| Insert | Open or close the in-game Lumen menu |
| Escape while the menu is open | Return to gameplay |
| X | Toggle Xray |
| Hold C | Zoom; release to restore the normal view |
| B | Toggle Fullbright |

The menu supports ore filters, a 4–24 block radius, 32–512 markers and 2×–20×
zoom. Settings save automatically. The menu does not pause the world.

All three features work on the client; no OP permission or chat command is
needed. Xray can only inspect block data the server actually sends. It cannot
recover ores hidden or substituted by a server. Follow the rules of servers
you join.

## Automatic updates

Installed copies check this repository's latest published release whenever
the launcher opens. A newer installer is downloaded over HTTPS and verified
against the size and SHA-256 digest in GitHub's release metadata before it runs.

Updates wait until Minecraft is closed. Lumen never closes Minecraft for you.
The installer waits for the launcher to exit, checks for locked files, updates
the same installation and reopens the launcher. Settings are preserved.
If GitHub is unavailable, you can still launch the installed version.

Updates run when the starter opens; there is no always-running update service.
Portable development copies without `lumen-install.ini` do not auto-install.
To opt out, create `%LOCALAPPDATA%\Lumen\updates.ini` containing:

```ini
[Updates]
Enabled=0
```

## Development and releases

Every push to **main** builds and tests on Windows. A successful current build
publishes a release numbered `1.4.<workflow run number>`. Pull requests build
and test without publishing. Incomplete uploads remain drafts and are not
eligible for automatic updates. Older queued commits cannot replace a newer
main build as the latest release.

See [BUILD.md](BUILD.md) for local builds and packaging, and
[TESTING.md](TESTING.md) for validation and runtime acceptance limits.

## Files and diagnostics

- Settings: `%LOCALAPPDATA%\Lumen\settings.ini`
- Game log: `%LOCALAPPDATA%\Lumen\lumen.log`
- Updater log: `%LOCALAPPDATA%\Lumen\Updates\updater.log`
- Automatic installer log: `%LOCALAPPDATA%\Lumen\Updates\setup.log`
- Closing the launcher after loading leaves Lumen running inside Minecraft.

## Source and credits

Lumen is distributed under **GPL-3.0-only**. Each release includes the complete
corresponding source in **Lumen-Source.zip**, including build scripts and bundled
dependencies. Windows and Visual Studio provide the platform toolchain.

Lumen adapts Minecraft integration, rendering and input techniques from
[Latite Client](https://github.com/LatiteClient/Latite). See
[LATITE-ATTRIBUTION.md](LATITE-ATTRIBUTION.md), [NOTICE.md](NOTICE.md) and
[LICENSE](LICENSE). MinHook, Geist and nlohmann/json retain their own licenses.

[In-game Figma design](https://www.figma.com/design/Lfm0C6WFqEGv6z6XLKszSW?node-id=3-2)
 · [Launcher Figma design](https://www.figma.com/design/Lfm0C6WFqEGv6z6XLKszSW?node-id=11-17)
