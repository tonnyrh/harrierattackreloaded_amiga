#Requires -Version 5.1
$ErrorActionPreference='Stop'
# One source tree for campaign and direct-entry test disks.
& powershell -NoProfile -File (Join-Path $PSScriptRoot 'amiga-build.ps1') -Program 'out/oil-platform/harrier_amiga' -ExtraCcFlags '-DHAR_OIL_PLATFORM_PROTOTYPE=1'
if($LASTEXITCODE -ne 0) {throw 'Oil-platform build failed'}
$source=Join-Path $PSScriptRoot 'amiga/out/oil-platform/harrier_amiga.adf'
$destinations=@('amiga/out/harrier_oil_platform.adf', '.tmp/oil-platform-prototype/amiga/out/harrier_oil_platform.adf', '.tmp/oil-platform-prototype/amiga/out/harrier_oil_platform_alpha_fixed.adf')
foreach($relative in $destinations) {
 $target=Join-Path $PSScriptRoot $relative
 if(Test-Path -LiteralPath (Split-Path $target)) {Copy-Item -LiteralPath $source -Destination $target -Force}
}
Write-Host 'Oil test disk updated from the current campaign sources, including legacy test aliases.'