#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$OutputRoot = "amiga\assets\generated\cpc",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$RepoRoot = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$Extractor = Join-Path $RepoRoot "tools\extract_cpc_assets.py"
$FontSource = Join-Path $RepoRoot "AMSTRADFONT3.asm"
$MainSource = Join-Path $RepoRoot "HarrierAttackSourceNew2_alt_CRTC_CART16.asm"
$OutputPath = if ([System.IO.Path]::IsPathRooted($OutputRoot)) {
    $OutputRoot
} else {
    Join-Path $RepoRoot $OutputRoot
}

if (-not (Test-Path -LiteralPath $Extractor)) {
    throw "Fant ikke CPC asset extractor: $Extractor"
}

if ($Clean -and (Test-Path -LiteralPath $OutputPath)) {
    $resolvedRepo = (Resolve-Path -LiteralPath $RepoRoot).Path
    $resolvedOutput = (Resolve-Path -LiteralPath $OutputPath).Path
    if (-not $resolvedOutput.StartsWith($resolvedRepo, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Nekter å slette utenfor repoet: $resolvedOutput"
    }
    if (-not $resolvedOutput.Contains("\amiga\assets\generated\cpc")) {
        throw "Nekter å slette uventet mappe: $resolvedOutput"
    }
    Remove-Item -LiteralPath $resolvedOutput -Recurse -Force
}

python $Extractor --font-source $FontSource --main-source $MainSource --out-root $OutputPath
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "CPC asset audit er klar: $OutputPath"
Write-Host "Se previews under: $(Join-Path $OutputPath 'previews')"
