$ErrorActionPreference = 'Stop'
$compilerRoot = Join-Path $PSScriptRoot '..\.tools\inno'
$compiler = Join-Path $compilerRoot 'ISCC.exe'
if (Test-Path -LiteralPath $compiler) { Write-Output (Resolve-Path $compiler).Path; exit 0 }
$toolsRoot = Split-Path $compilerRoot
New-Item -ItemType Directory -Path $toolsRoot -Force | Out-Null
$installer = Join-Path $toolsRoot 'innosetup-7.1.0-x64.exe'
Invoke-WebRequest 'https://github.com/jrsoftware/issrc/releases/download/is-7_1_0/innosetup-7.1.0-x64.exe' -OutFile $installer
$expected = '0362a383ed217d4c4239b5933866dd96d3eb2102737da92f80f6057a4b40df2f'
if ((Get-FileHash $installer -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'Inno Setup compiler checksum mismatch' }
if ((Get-AuthenticodeSignature $installer).Status -ne 'Valid') { throw 'Inno Setup compiler signature is not valid' }
$compilerRoot = [IO.Path]::GetFullPath($compilerRoot)
$arguments = '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CURRENTUSER /NOICONS /TASKS="" /DIR="' + $compilerRoot + '"'
$process = Start-Process -FilePath $installer -ArgumentList $arguments -PassThru -WindowStyle Hidden
$process.WaitForExit()
if ($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath $compiler)) { throw 'Inno Setup compiler installation failed' }
Write-Output (Resolve-Path $compiler).Path
