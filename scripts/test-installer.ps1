param([string]$Distribution = (Join-Path $PSScriptRoot '..\dist'), [string]$TestRoot = '')
$ErrorActionPreference = 'Stop'
$distributionPath = (Resolve-Path $Distribution).Path
if (!$TestRoot) { $TestRoot = Join-Path $env:TEMP ('LumenInstallerQA-' + [guid]::NewGuid().ToString('N')) }
$testPath = [IO.Path]::GetFullPath($TestRoot)
if (Test-Path -LiteralPath $testPath) { throw 'Installer test requires a new, empty target path' }
$uninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{1F0181CF-851F-4E47-9252-FF53703882AC}_is1'
if (Test-Path $uninstallKey) { throw 'An installed Lumen copy exists; refusing to replace its registration for this test' }
$settings = Join-Path $env:LOCALAPPDATA 'Lumen\settings.ini'
$before = if (Test-Path -LiteralPath $settings) { (Get-FileHash -LiteralPath $settings -Algorithm SHA256).Hash } else { $null }
$arguments = '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /NOICONS /TASKS="" /DIR="' + $testPath + '"'
$setup = Start-Process -FilePath (Join-Path $distributionPath 'Lumen-Setup.exe') -ArgumentList $arguments -PassThru -WindowStyle Hidden
$setup.WaitForExit()
if ($setup.ExitCode -ne 0) { throw ('Installer test failed: ' + $setup.ExitCode) }
foreach ($name in @('Lumen.exe','Lumen.dll','assets\Geist-Regular.ttf','assets\Geist-SemiBold.ttf','LICENSE','SOURCE.md','JSON-LICENSE.txt')) {
  $installed = Get-FileHash -LiteralPath (Join-Path $testPath $name) -Algorithm SHA256
  $payload = Get-FileHash -LiteralPath (Join-Path $distributionPath ('payload\' + $name)) -Algorithm SHA256
  if ($installed.Hash -ne $payload.Hash) { throw ('Installed file differs: ' + $name) }
}
if (!(Test-Path -LiteralPath (Join-Path $testPath 'lumen-install.ini'))) { throw 'Installed-copy marker is missing' }
# Hold only this test installation's DLL open; no Minecraft process is touched.
$locked = [IO.File]::Open((Join-Path $testPath 'Lumen.dll'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try {
  $blocked = Start-Process -FilePath (Join-Path $distributionPath 'Lumen-Setup.exe') -ArgumentList $arguments -PassThru -WindowStyle Hidden
  $blocked.WaitForExit()
  if ($blocked.ExitCode -eq 0) { throw 'Installer unexpectedly accepted a locked DLL' }
} finally { $locked.Dispose() }
# Updating the same version must preserve the installed bytes and registration.
$repeat = Start-Process -FilePath (Join-Path $distributionPath 'Lumen-Setup.exe') -ArgumentList $arguments -PassThru -WindowStyle Hidden
$repeat.WaitForExit()
if ($repeat.ExitCode -ne 0) { throw 'Reinstallation failed' }
$uninstaller = [IO.Path]::GetFullPath((Join-Path $testPath 'unins000.exe'))
if (!$uninstaller.StartsWith($testPath + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Uninstaller escaped the checked test directory' }
$uninstall = Start-Process -FilePath $uninstaller -ArgumentList '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART' -PassThru -WindowStyle Hidden
$uninstall.WaitForExit()
if ($uninstall.ExitCode -ne 0 -or (Test-Path -LiteralPath (Join-Path $testPath 'Lumen.dll')) -or (Test-Path $uninstallKey)) { throw 'Uninstall verification failed' }
$after = if (Test-Path -LiteralPath $settings) { (Get-FileHash -LiteralPath $settings -Algorithm SHA256).Hash } else { $null }
if ($before -ne $after) { throw 'Existing Lumen settings changed' }
Write-Output 'INSTALLER_SMOKE_OK: install, payload hashes, locked-file refusal, reinstall, uninstall, settings preserved'
