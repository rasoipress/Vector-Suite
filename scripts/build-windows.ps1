# Compila il plug-in Vector Suite per Windows.
#
#   .\scripts\build-windows.ps1
#   .\scripts\build-windows.ps1 -Configuration Debug
#
# Prerequisiti: Visual Studio 2022 con il carico "Sviluppo di applicazioni
# desktop con C++", CMake, Python 3, e l'Adobe Illustrator SDK scompattato in
# sdk\Adobe Illustrator 2026 SDK.

[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$sdkPath = Join-Path $projectRoot 'sdk\Adobe Illustrator 2026 SDK'
$sourceDir = Join-Path $projectRoot 'native\VectorSuiteNative'
$buildDir = Join-Path $projectRoot 'build\win'

if (-not (Test-Path (Join-Path $sdkPath 'illustratorapi'))) {
    Write-Error @"
Manca l'Adobe Illustrator SDK.

  Atteso in:  sdk\Adobe Illustrator 2026 SDK\

  L'SDK non è incluso nel progetto: è coperto dalla licenza Adobe e non si può
  ridistribuire. Va scaricato da Adobe, gratuitamente, con un Adobe ID:

    https://developer.adobe.com/console/servicesandapis/ai
    https://developer.adobe.com/console/downloads

  Le istruzioni complete sono in sdk\README.md.
"@
}

Write-Host '== Applico la versione dal file VERSION'
python (Join-Path $projectRoot 'scripts\apply-version.py')

Write-Host '== Rigenero icone e mappa delle risorse'
python (Join-Path $projectRoot 'scripts\generate-assets.py')

Write-Host "== Configuro ($Configuration, x64)"
cmake -S $sourceDir -B $buildDir -A x64
if ($LASTEXITCODE -ne 0) { throw 'Configurazione CMake non riuscita.' }

Write-Host '== Compilo'
cmake --build $buildDir --config $Configuration
if ($LASTEXITCODE -ne 0) { throw 'Compilazione non riuscita.' }

$plugin = Join-Path $buildDir "$Configuration\VectorSuiteNative.aip"
if (-not (Test-Path $plugin)) { throw "Il plug-in non è stato prodotto: $plugin" }

Write-Host ''
Write-Host 'Plug-in pronto:'
Write-Host "  $plugin"
Write-Host 'Per installarlo:  .\scripts\install-windows.ps1'
