"""Build the per-user installer, download ZIP and corresponding source archive."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import zipfile

p = argparse.ArgumentParser()
p.add_argument('--version', required=True)
p.add_argument('--build', default='build')
p.add_argument('--output', default='dist')
p.add_argument('--iscc', required=True)
p.add_argument('--commit', default='local')
args = p.parse_args()
if not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
    raise SystemExit('Invalid version')
root = Path(__file__).resolve().parent.parent
build = (root / args.build).resolve()
out = (root / args.output).resolve()
payload = out / 'payload'
payload.mkdir(parents=True, exist_ok=True)
for name in ('Lumen.exe', 'Lumen.dll'):
    shutil.copy2(build / name, payload / name)
shutil.copytree(root / 'assets', payload / 'assets', dirs_exist_ok=True)
for name in ('LICENSE', 'NOTICE.md', 'LATITE-ATTRIBUTION.md', 'README.md'):
    shutil.copy2(root / name, payload / name)
shutil.copy2(root / 'third_party/nlohmann/LICENSE.MIT', payload / 'JSON-LICENSE.txt')
shutil.copy2(root / 'third_party/minhook/LICENSE.txt', payload / 'MINHOOK-LICENSE.txt')
release_url = f'https://github.com/AuronNetwork/Lumen/releases/tag/v{args.version}'
(payload / 'SOURCE.md').write_text(f'''# Corresponding source for Lumen {args.version}

Download **Lumen-Source.zip** from [{release_url}]({release_url}).
It contains all application and bundled dependency source, the build scripts,
and the GPL and dependency licenses. This source corresponds to commit
`{args.commit}`. Build instructions are in BUILD.md.
''', encoding='utf-8')
(payload / 'release.json').write_text(json.dumps({'version': args.version, 'commit': args.commit, 'repository': 'AuronNetwork/Lumen'}, indent=2), encoding='utf-8')
major, minor, patch = map(int, args.version.split('.'))
subprocess.run([args.iscc, f'/DAppVersion={args.version}', f'/DVersionMS={(major << 16) | minor}', f'/DVersionLS={patch << 16}', f'/DPayloadDir={payload}', f'/DOutputPath={out}', str(root / 'installer/Lumen.iss')], check=True)
source = out / 'Lumen-Source.zip'
excluded = {'.git', 'build', 'dist', '.tools', '__pycache__'}
with zipfile.ZipFile(source, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for file in sorted(root.rglob('*')):
        rel = file.relative_to(root)
        if file.is_file() and rel.parts[0] not in excluded and '.git' not in rel.parts and '__pycache__' not in rel.parts and out not in file.parents:
            z.write(file, 'Lumen/' + rel.as_posix())
    z.writestr('Lumen/RELEASE-VERSION.txt', args.version + '\n')
    z.writestr('Lumen/RELEASE-COMMIT.txt', args.commit + '\n')
manual = '''Lumen installer for Minecraft Bedrock 26.45 (Windows x64)

1. Extract this ZIP.
2. Run Lumen-Setup.exe. No administrator rights are required.
3. Save and close Minecraft if another Lumen version is already loaded.
4. Open Lumen and select Launch Lumen. Insert opens the in-game menu.

The installer places the program in your user profile, adds a Start-menu
shortcut and provides an uninstaller. Settings remain in %LOCALAPPDATA%\\Lumen.
Installed copies check GitHub for updates when the launcher opens. An update
is installed automatically only when Minecraft is closed. The installer never
forces the game to exit. An offline update check does not block launching.

This installer is not Authenticode-signed. Windows may display an unknown
publisher or SmartScreen warning. Release assets include SHA-256 checksums.
Source and licenses: https://github.com/AuronNetwork/Lumen
'''
bundle = out / 'Lumen-Installer.zip'
with zipfile.ZipFile(bundle, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    z.write(out / 'Lumen-Setup.exe', 'Lumen-Setup.exe')
    z.writestr('INSTALL.txt', manual)
    for name in ('LICENSE', 'NOTICE.md'):
        z.write(root / name, name)
    z.writestr('SOURCE.md', (payload / 'SOURCE.md').read_text(encoding='utf-8'))
for archive in (bundle, source):
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
with zipfile.ZipFile(bundle) as z:
    assert z.read('Lumen-Setup.exe') == (out / 'Lumen-Setup.exe').read_bytes()
assets = (out / 'Lumen-Setup.exe', bundle, source)
(out / 'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(file.read_bytes()).hexdigest() + '  ' + file.name + '\n' for file in assets), encoding='utf-8')
print('PACKAGE_OK', args.version)
for file in assets:
    print(file.name, file.stat().st_size, hashlib.sha256(file.read_bytes()).hexdigest())
