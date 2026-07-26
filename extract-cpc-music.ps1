[CmdletBinding()]
param(
    [string]$Output = "amiga/assets/music/harrier_menu.mod",
    [switch]$FourChannelArrangement
)

$ErrorActionPreference = "Stop"
$repoRoot = $PSScriptRoot

Push-Location $repoRoot
try {
    $arguments = @("tools/cpc_music_to_mod.py", "--output", $Output)
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
