# Deploys Qt next to AnyPC.exe and builds the Inno Setup installer.
# Usage: packaging/windows/build-installer.ps1 -BuildDir build -Version 1.2.3 -OutputDir dist
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$OutputDir
)
$ErrorActionPreference = 'Stop'

$BuildDir = (Resolve-Path $BuildDir).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = (Resolve-Path $OutputDir).Path
$Staging = Join-Path $BuildDir 'deploy'

if (Test-Path $Staging) { Remove-Item -Recurse -Force $Staging }
New-Item -ItemType Directory -Force -Path $Staging | Out-Null
Copy-Item (Join-Path $BuildDir 'AnyPC.exe') $Staging

& windeployqt --release --compiler-runtime (Join-Path $Staging 'AnyPC.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed ($LASTEXITCODE)" }

$iscc = (Get-Command iscc.exe -ErrorAction SilentlyContinue).Source
if (-not $iscc) { $iscc = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe' }
if (-not (Test-Path $iscc)) { throw 'Inno Setup (ISCC.exe) not found' }

$script = Join-Path $PSScriptRoot 'installer.iss'
& $iscc "/DAppVersion=$Version" "/DSourceDir=$Staging" "/DOutputDir=$OutputDir" $script
if ($LASTEXITCODE -ne 0) { throw "ISCC failed ($LASTEXITCODE)" }
