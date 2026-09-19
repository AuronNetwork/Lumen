# Build Lumen

Requirements: Windows x64, Visual Studio 2022 C++ tools, Windows SDK, CMake
3.22+, and Python 3 for packaging. Local builds were tested with MSVC 14.44.

From the repository root:

```powershell
cmake -S . -B build -A x64 "-DLUMEN_VERSION=1.6.0"
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

For a source ZIP, replace 1.6.0 with the version in RELEASE-VERSION.txt.
The default updater repository is AuronNetwork/Lumen. Forks can set
`-DLUMEN_REPOSITORY=owner/repository`; adapt installer/package URLs as well
before distributing a fork. Never point another project's updater at Lumen.

For Ninja, use an x64 Native Tools prompt, omit `-A x64`, add
`-G Ninja -DCMAKE_BUILD_TYPE=Release`, and omit `--config Release`.

The build produces Lumen.exe, Lumen.dll and optional Lumen.pdb symbols.
Place assets beside the executable and DLL for a portable development copy.
MinHook, nlohmann/json and Geist are vendored; CMake does not download code.
Windows graphics, WinHTTP and CNG are platform dependencies. There is no
.NET or browser runtime in the launcher.

## Installer and ZIPs

Build first, then run in PowerShell:

```powershell
$iscc = & ./scripts/bootstrap-inno.ps1
python scripts/package.py --version 1.6.0 --build build/Release --iscc $iscc --commit (git rev-parse HEAD)
```

For Ninja use `--build build`. The bootstrap pins Inno Setup 7.1.0, validates
its hash and Authenticode signature, then installs the compiler into the
ignored .tools/inno directory for the current user. An existing Inno 7 compiler
can instead be passed directly with --iscc.

dist contains Lumen-Setup.exe, Lumen-Installer.zip, Lumen-Source.zip and
SHA256SUMS.txt. The installer installs runtime files, licenses and documentation,
and links to the exact release's complete source archive. Debug symbols are
not installed.

## Release automation

.github/workflows/release.yml publishes a complete release after a successful
push to main. Version 1.6.<github.run_number> is compiled into both binaries,
their Windows resources and the installer. Components must fit 0â€“65535.
To start a new version series, update the workflow prefix and CMake default.

Build/test jobs have read-only repository permissions. Only the publication
job gets contents:write. Actions are pinned to commits. Assets are uploaded
to a draft and published only after uploads succeed and main still points to
the tested commit. Published versions are never overwritten.

## Test helpers

`updater_io_tests.exe --download` downloads the real latest release installer
and validates its digest without executing it. Without arguments it checks
SHA-256 against a known fixture. The policy test covers strict version ordering,
malformed metadata, untrusted URLs and invalid digests.

`cmake --build build --config Release --target launcher_preview` builds a UI
preview without the game loader. Copy assets beside it. Its eight-second task
ends in Loaded by default; error and uncertain arguments choose other states.
It is excluded from release packages and is not a Minecraft test.
