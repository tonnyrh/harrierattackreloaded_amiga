#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$CpcSourceRoot,
    [switch]$NoOpen,
    [int]$Scale = 6
)

$ErrorActionPreference = "Stop"
$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$Script = Join-Path $Root "tools\view_cpc_graphics.py"

if (-not (Test-Path -LiteralPath $Script)) {
    throw "Fant ikke $Script"
}

$auditRoot = Join-Path $Root ".tmp\cpc-asset-audit"
$mainSource = Join-Path (Resolve-Path -LiteralPath $CpcSourceRoot -ErrorAction Stop).Path "HarrierAttackSourceNew2_alt_CRTC_CART16.asm"
$argsList = @($Script, "--source-root", $auditRoot, "--main-source", $mainSource, "--scale", "$Scale")
if (-not $NoOpen) {
    $argsList += "--open"
}

& python @argsList
exit $LASTEXITCODE
