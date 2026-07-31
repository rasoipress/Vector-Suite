# Disinstalla il plug-in Vector Suite da Adobe Illustrator su Windows,
# conservandone una copia recuperabile.
#
#   .\scripts\uninstall-windows.ps1

[CmdletBinding()]
param([string]$PluginFolder)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

if (-not $PluginFolder) {
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

$target = Join-Path $PluginFolder 'VectorSuiteNative.aip'
if (-not (Test-Path $target)) {
    Write-Host "Vector Suite non è installato in $PluginFolder"
    exit 0
}

$backupRoot = Join-Path $projectRoot 'backups\uninstalled'
New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backup = Join-Path $backupRoot "VectorSuiteNative-$stamp.aip"

Move-Item $target $backup

Write-Host 'Plug-in rimosso da Illustrator e conservato in:'
Write-Host "  $backup"
