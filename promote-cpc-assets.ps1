#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GeneratedRoot = "amiga\assets\generated\cpc",
    [string]$OutputHeader = "amiga\assets\cpc_promoted_assets.h"
)

$ErrorActionPreference = "Stop"

$RepoRoot = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$Promoter = Join-Path $RepoRoot "tools\promote_cpc_assets.py"
$GeneratedPath = if ([System.IO.Path]::IsPathRooted($GeneratedRoot)) {
    $GeneratedRoot
} else {
    Join-Path $RepoRoot $GeneratedRoot
}
$InputJson = Join-Path $GeneratedPath "cpc_plus_sprites.json"
$ObjectsJson = Join-Path $GeneratedPath "cpc_object_blocks.json"
$OutputPath = if ([System.IO.Path]::IsPathRooted($OutputHeader)) {
    $OutputHeader
} else {
    Join-Path $RepoRoot $OutputHeader
}

if (-not (Test-Path -LiteralPath $InputJson)) {
    $ExtractorScript = Join-Path $RepoRoot "extract-cpc-assets.ps1"
    if (-not (Test-Path -LiteralPath $ExtractorScript)) {
        throw "Fant ikke generated CPC sprites eller extractor: $InputJson"
    }
    & $ExtractorScript
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

python $Promoter --input $InputJson --objects-input $ObjectsJson --output $OutputPath
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Promoted CPC runtime assets er klar: $OutputPath"
