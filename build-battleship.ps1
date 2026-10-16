#Requires -Version 5.1
$ErrorActionPreference='Stop'
# Build direct-entry tests from the same sources and staged loader as the campaign.
& powershell -NoProfile -File (Join-Path $PSScriptRoot 'amiga-build.ps1') -Program 'out/battleship/harrier_amiga' -ExtraCcFlags '-DHAR_BATTLESHIP_PROTOTYPE=1'
if($LASTEXITCODE -ne 0) {throw 'Battleship build failed'}
$source=Join-Path $PSScriptRoot 'amiga/out/battleship/harrier_amiga.adf'
foreach($relative in @('amiga/out/harrier_battleship.adf','.tmp/battleship-prototype/amiga/out/harrier_battleship.adf')) {
 $target=Join-Path $PSScriptRoot $relative
 if(Test-Path -LiteralPath (Split-Path $target)) {Copy-Item -LiteralPath $source -Destination $target -Force}
}
Write-Host 'Battleship test disk updated from current campaign sources.'
