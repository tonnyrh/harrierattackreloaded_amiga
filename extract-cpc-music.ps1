[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$CpcSourceRoot,
    [string]$Output = "amiga/assets/music/harrier_menu.mod",
    [switch]$FourChannelArrangement
)

$ErrorActionPreference = "Stop"
$repoRoot = $PSScriptRoot

Push-Location $repoRoot
try {
    $input = Join-Path (Resolve-Path -LiteralPath $CpcSourceRoot -ErrorAction Stop).Path "CPSoundEffectGenerator2.asm"
    $arguments = @("tools/cpc_music_to_mod.py", "--input", $input, "--output", $Output)
    if ($FourChannelArrangement) {
        $arguments += "--four-channel-arrangement"
    }
    python @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "CPC music conversion failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}
