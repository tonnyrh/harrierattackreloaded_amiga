#Requires -Version 5.1
[CmdletBinding()]
param(
    [switch]$NoOpen,
    [int]$Scale = 6
)

$ErrorActionPreference = "Stop"
$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$Script = Join-Path $Root "tools\view_cpc_graphics.py"

if (-not (Test-Path -LiteralPath $Script)) {
    throw "Fant ikke $Script"
}

$argsList = @($Script, "--scale", "$Scale")
if (-not $NoOpen) {
    $argsList += "--open"
}

& python @argsList
exit $LASTEXITCODE
