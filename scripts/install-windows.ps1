# Installa il plug-in Vector Suite in Adobe Illustrator su Windows.
#
#   .\scripts\install-windows.ps1
#   .\scripts\install-windows.ps1 -PluginFolder "C:\Program Files\Adobe\Adobe Illustrator 2026\Plug-ins"
#
# La cartella dei plug-in di Illustrator sta sotto Programmi, quindi serve una
# console di PowerShell aperta come amministratore. Chiudere Illustrator prima.

[CmdletBinding()]
param(
    [string]$PluginFolder,
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$plugin = Join-Path $projectRoot "build\win\$Configuration\VectorSuiteNative.aip"

if (-not (Test-Path $plugin)) {
    Write-Error "Manca il plug-in compilato. Lancia prima .\scripts\build-windows.ps1"
}

if (-not $PluginFolder) {
    # Sceglie l'installazione più recente fra quelle presenti.
    $candidates = @()
    foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if (-not $base) { continue }
        $adobe = Join-Path $base 'Adobe'
        if (-not (Test-Path $adobe)) { continue }
        $candidates += Get-ChildItem $adobe -Directory -Filter 'Adobe Illustrator*' |
            ForEach-Object { Join-Path $_.FullName 'Plug-ins' } |
            Where-Object { Test-Path $_ }
    }
    $PluginFolder = $candidates | Sort-Object -Descending | Select-Object -First 1
}

if (-not $PluginFolder -or -not (Test-Path $PluginFolder)) {
    Write-Error @"
Cartella Plug-ins di Illustrator non trovata.

  Indicala a mano, per esempio:
    .\scripts\install-windows.ps1 -PluginFolder "C:\Program Files\Adobe\Adobe Illustrator 2026\Plug-ins"
"@
}

$destination = Join-Path $PluginFolder 'VectorSuiteNative.aip'

# Come su macOS: la versione precedente non viene cancellata ma conservata.
if (Test-Path $destination) {
    $backupRoot = Join-Path $projectRoot 'backups\native'
    New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    Copy-Item $destination (Join-Path $backupRoot "VectorSuiteNative-$stamp.aip") -Force
    Write-Host "Versione precedente conservata in backups\native"
}

Copy-Item $plugin $destination -Force

Write-Host ''
Write-Host 'Vector Suite installato in:'
Write-Host "  $destination"
Write-Host 'Riavvia Illustrator e apri Finestra > Vector Suite.'
