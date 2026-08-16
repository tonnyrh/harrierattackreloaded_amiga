#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet("build", "clean")]
    [string]$Target = "build",
    [string]$Program = "out/harrier_amiga"
)

$ErrorActionPreference = "Stop"

function Ensure-Directory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Set-UaeConfigOption {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $lines = @(Get-Content -LiteralPath $Path)
    $prefix = "$Name="
    $replacement = "$Name=$Value"
    $found = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i].StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            $lines[$i] = $replacement
            $found = $true
        }
    }
    if (-not $found) {
        $lines += $replacement
    }

    Set-Content -LiteralPath $Path -Value $lines -Encoding ASCII
}

$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$AmigaDir = Join-Path $Root "amiga"
$ExtensionRoot = Join-Path $env:USERPROFILE ".vscode\extensions"
$AmigaSfxPipeline = Join-Path $Root "prepare-amiga-sfx.ps1"
$LoadingBpl = Join-Path $AmigaDir "assets\loading_screen.bpl"
$LoadingPal = Join-Path $AmigaDir "assets\loading_screen.pal"
$FontBin = Join-Path $AmigaDir "assets\font8x8.bin"
$MenuTextHeader = Join-Path $AmigaDir "assets\harrier_menu_text.h"
$GameTiles = Join-Path $AmigaDir "assets\game_tiles.bpl"
$GameSceneMap = Join-Path $AmigaDir "assets\game_scene.map"
$GamePalette = Join-Path $AmigaDir "assets\game_palette.pal"
$PromotedHeader = Join-Path $AmigaDir "assets\promoted_assets.h"
$PromotedSpriteTiles = Join-Path $AmigaDir "assets\promoted_sprite_tiles.h"

if (-not (Test-Path -LiteralPath $AmigaDir)) {
    throw "Fant ikke Amiga-mappen: $AmigaDir"
}


$extension = Get-ChildItem -Directory $ExtensionRoot -Filter "bartmanabyss.amiga-debug-*" -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    Select-Object -First 1

if (-not $extension) {
    throw "Fant ikke VS Code-extension BartmanAbyss.amiga-debug under $ExtensionRoot. Kjor .\setup-dev-amiga.ps1 forst."
}

$bin = Join-Path $extension.FullName "bin\win32"
$make = Join-Path $bin "gnumake.exe"
$Dh0LogsDir = Join-Path $extension.FullName "bin\dh0\logs"
$WinUaeConfig = Join-Path $bin "default.uae"

if (-not (Test-Path -LiteralPath $make)) {
    throw "Fant ikke gnumake.exe: $make"
}

Ensure-Directory -Path $Dh0LogsDir

# Gameplay uses the standard OCS fine-scroll fetch: 336 pixels are fetched so
# the 320-pixel DIW can move smoothly by 0..15 pixels.  Without WinUAE's
# automatic display crop, the hidden fetch word is exposed as an 8-pixel
# COLOR00 border on both sides and looks like an empty map tile.
Set-UaeConfigOption -Path $WinUaeConfig -Name "gfx_filter_autoscale" -Value "auto"

# Prefer the toolchain shipped with the selected Bartman extension over any
# unrelated make/compiler installation already present on the host.
$env:PATH = "$bin\opt\bin;$bin;$env:PATH"

if ($Target -eq "clean") {
    & $make -C $AmigaDir clean
    exit $LASTEXITCODE
}

# This repository is the standalone Amiga port. Converted, build-ready assets
# are versioned under amiga/assets; the original CPC source and its extraction
# pipeline deliberately live outside this repository.
$RequiredAmigaAssets = @(
    $LoadingBpl,
    $LoadingPal,
    $FontBin,
    $MenuTextHeader,
    $GameTiles,
    $GameSceneMap,
    $GamePalette,
    $PromotedHeader,
    $PromotedSpriteTiles
)
foreach ($asset in $RequiredAmigaAssets) {
    if (-not (Test-Path -LiteralPath $asset -PathType Leaf)) {
        throw "Mangler versjonert Amiga-asset: $asset"
    }
}

if (Test-Path -LiteralPath $AmigaSfxPipeline) {
    & $AmigaSfxPipeline
    if (-not $?) {
        throw "Amiga-lydpipelinen feilet: $AmigaSfxPipeline"
    }
}

# The parity/headless runner supplies EXTRA_CCFLAGS directly to make. GNU make
# does not consider command-line flag changes when deciding whether main.o is
# current, so an interrupted diagnostic run could otherwise leave F5 launching
# an autoplay/test binary. Force just the translation unit that contains those
# compile-time switches; support objects remain incremental.
$MainObject = Join-Path $AmigaDir "obj\main.o"
if (Test-Path -LiteralPath $MainObject) {
    Remove-Item -LiteralPath $MainObject -Force
}

& $make -C $AmigaDir -j4 "program=$Program"
$makeExitCode = $LASTEXITCODE

if ($makeExitCode -eq 0) {
    $Exe2Adf = Join-Path $bin "exe2adf.exe"
    $BuiltExe = Join-Path $AmigaDir "$Program.exe"
    $AdfPath = Join-Path $AmigaDir "$Program.adf"
    $ProgramDir = Split-Path -Parent $BuiltExe
    $RuntimeLoadingBpl = Join-Path $ProgramDir "loading_screen.bpl"
    $AdfAssetsDir = Join-Path $ProgramDir "adf-assets"

    # Sprint 15.61: the loading bitmap is a runtime file rather than a
    # permanent 40 KiB EMBED_CHIP object. Keep it beside the executable for
    # Bartman/F5 (DH1:) and include the same file at the root of the ADF.
    Ensure-Directory -Path $ProgramDir
    Copy-Item -LiteralPath $LoadingBpl -Destination $RuntimeLoadingBpl -Force
    Ensure-Directory -Path $AdfAssetsDir
    Copy-Item -LiteralPath $LoadingBpl -Destination (Join-Path $AdfAssetsDir "loading_screen.bpl") -Force

    # Keep the release boot path deliberately simple on a stock 512 KiB Chip
    # + 512 KiB Slow machine: AmigaDOS loads the game directly, and main()
    # displays loading_screen.bpl after startup. A separate loader would keep
    # its process/stack resident while LoadSeg allocates the game hunks and can
    # make an otherwise valid 1 MiB build fail or return to the CLI.
    foreach ($staleAdfAsset in @(
        "harrier_amiga.exe",
        "harrier_loader.exe",
        "loading_screen_2plane.bpl",
        "run_harrier"
    )) {
        $stalePath = Join-Path $AdfAssetsDir $staleAdfAsset
        if (Test-Path -LiteralPath $stalePath) {
            Remove-Item -LiteralPath $stalePath -Force
        }
    }

    if ((Test-Path -LiteralPath $Exe2Adf) -and (Test-Path -LiteralPath $BuiltExe)) {
        & $Exe2Adf -i $BuiltExe -l "Harrier Attack" -a $AdfPath -d $AdfAssetsDir
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "exe2adf feilet (kode $LASTEXITCODE) - ADF ble ikke generert."
        }
    } else {
        Write-Warning "Fant ikke exe2adf.exe eller $BuiltExe - hopper over ADF-generering."
    }
}

exit $makeExitCode
