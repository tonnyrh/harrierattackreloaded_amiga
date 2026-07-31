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

$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$AmigaDir = Join-Path $Root "amiga"
$ExtensionRoot = Join-Path $env:USERPROFILE ".vscode\extensions"
$Converter = Join-Path $Root "tools\cpc_screen_to_amiga.py"
$FontConverter = Join-Path $Root "tools\cpc_font_to_amiga.py"
$GameTileConverter = Join-Path $Root "tools\cpc_game_tiles_to_amiga.py"
$CpcAssetExtractorScript = Join-Path $Root "extract-cpc-assets.ps1"
$CpcAssetPromoterScript = Join-Path $Root "promote-cpc-assets.ps1"
$CpcAssetExtractorPy = Join-Path $Root "tools\extract_cpc_assets.py"
$CpcAssetPromoterPy = Join-Path $Root "tools\promote_cpc_assets.py"
$AmigaSfxPipeline = Join-Path $Root "prepare-amiga-sfx.ps1"
$CpcLoadingScreen = Join-Path $Root "compile\HARRSCR.bin"
$PaletteSource = Join-Path $Root "boot2.asm"
$FontSource = Join-Path $Root "AMSTRADFONT3.asm"
$MainSource = Join-Path $Root "HarrierAttackSourceNew2_alt_CRTC_CART16.asm"
$LoadingBpl = Join-Path $AmigaDir "assets\loading_screen.bpl"
$LoadingPal = Join-Path $AmigaDir "assets\loading_screen.pal"
$FontBin = Join-Path $AmigaDir "assets\cpc_font8x8.bin"
$MenuTextHeader = Join-Path $AmigaDir "assets\harrier_menu_text.h"
$GameTiles = Join-Path $AmigaDir "assets\game_tiles.bpl"
$GameSceneMap = Join-Path $AmigaDir "assets\game_scene.map"
$GamePalette = Join-Path $AmigaDir "assets\game_palette.pal"
$CpcGeneratedPlusSprites = Join-Path $AmigaDir "assets\generated\cpc\cpc_plus_sprites.json"
$CpcPromotedHeader = Join-Path $AmigaDir "assets\cpc_promoted_assets.h"

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

if (-not (Test-Path -LiteralPath $make)) {
    throw "Fant ikke gnumake.exe: $make"
}

Ensure-Directory -Path $Dh0LogsDir

$env:PATH = "$env:PATH;$bin\opt\bin;$bin"

if ($Target -eq "clean") {
    & $make -C $AmigaDir clean
    exit $LASTEXITCODE
}

if (Test-Path -LiteralPath $Converter) {
    $needsAssets = -not (Test-Path -LiteralPath $LoadingBpl) -or -not (Test-Path -LiteralPath $LoadingPal)
    if (-not $needsAssets) {
        $assetTime = (Get-Item -LiteralPath $LoadingBpl).LastWriteTimeUtc
        $needsAssets = (Get-Item -LiteralPath $Converter).LastWriteTimeUtc -gt $assetTime
        if ((Test-Path -LiteralPath $CpcLoadingScreen) -and ((Get-Item -LiteralPath $CpcLoadingScreen).LastWriteTimeUtc -gt $assetTime)) {
            $needsAssets = $true
        }
        if ((Test-Path -LiteralPath $PaletteSource) -and ((Get-Item -LiteralPath $PaletteSource).LastWriteTimeUtc -gt $assetTime)) {
            $needsAssets = $true
        }
    }

    if ($needsAssets) {
        & python $Converter
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

if (Test-Path -LiteralPath $FontConverter) {
    $needsFontAssets = -not (Test-Path -LiteralPath $FontBin) -or -not (Test-Path -LiteralPath $MenuTextHeader)
    if (-not $needsFontAssets) {
        $fontAssetTime = (Get-Item -LiteralPath $FontBin).LastWriteTimeUtc
        $needsFontAssets = (Get-Item -LiteralPath $FontConverter).LastWriteTimeUtc -gt $fontAssetTime
        if ((Test-Path -LiteralPath $FontSource) -and ((Get-Item -LiteralPath $FontSource).LastWriteTimeUtc -gt $fontAssetTime)) {
            $needsFontAssets = $true
        }
    }

    if ($needsFontAssets) {
        & python $FontConverter
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

if (Test-Path -LiteralPath $GameTileConverter) {
    $needsGameAssets = -not (Test-Path -LiteralPath $GameTiles) -or -not (Test-Path -LiteralPath $GameSceneMap) -or -not (Test-Path -LiteralPath $GamePalette)
    if (-not $needsGameAssets) {
        $gameAssetTime = (Get-Item -LiteralPath $GameTiles).LastWriteTimeUtc
        $needsGameAssets = (Get-Item -LiteralPath $GameTileConverter).LastWriteTimeUtc -gt $gameAssetTime
        if ((Test-Path -LiteralPath $FontSource) -and ((Get-Item -LiteralPath $FontSource).LastWriteTimeUtc -gt $gameAssetTime)) {
            $needsGameAssets = $true
        }
    }

    if ($needsGameAssets) {
        & python $GameTileConverter
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

if (Test-Path -LiteralPath $CpcAssetExtractorScript) {
    $needsCpcAssetAudit = -not (Test-Path -LiteralPath $CpcGeneratedPlusSprites)
    if (-not $needsCpcAssetAudit) {
        $auditTime = (Get-Item -LiteralPath $CpcGeneratedPlusSprites).LastWriteTimeUtc
        $needsCpcAssetAudit = (Get-Item -LiteralPath $CpcAssetExtractorScript).LastWriteTimeUtc -gt $auditTime
        if ((Test-Path -LiteralPath $CpcAssetExtractorPy) -and ((Get-Item -LiteralPath $CpcAssetExtractorPy).LastWriteTimeUtc -gt $auditTime)) {
            $needsCpcAssetAudit = $true
        }
        if ((Test-Path -LiteralPath $FontSource) -and ((Get-Item -LiteralPath $FontSource).LastWriteTimeUtc -gt $auditTime)) {
            $needsCpcAssetAudit = $true
        }
        if ((Test-Path -LiteralPath $MainSource) -and ((Get-Item -LiteralPath $MainSource).LastWriteTimeUtc -gt $auditTime)) {
            $needsCpcAssetAudit = $true
        }
    }

    if ($needsCpcAssetAudit) {
        & $CpcAssetExtractorScript
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

if (Test-Path -LiteralPath $CpcAssetPromoterScript) {
    $needsCpcPromotedAssets = -not (Test-Path -LiteralPath $CpcPromotedHeader)
    if (-not $needsCpcPromotedAssets) {
        $promotedTime = (Get-Item -LiteralPath $CpcPromotedHeader).LastWriteTimeUtc
        $needsCpcPromotedAssets = (Get-Item -LiteralPath $CpcAssetPromoterScript).LastWriteTimeUtc -gt $promotedTime
        if ((Test-Path -LiteralPath $CpcAssetPromoterPy) -and ((Get-Item -LiteralPath $CpcAssetPromoterPy).LastWriteTimeUtc -gt $promotedTime)) {
            $needsCpcPromotedAssets = $true
        }
        if ((Test-Path -LiteralPath $CpcGeneratedPlusSprites) -and ((Get-Item -LiteralPath $CpcGeneratedPlusSprites).LastWriteTimeUtc -gt $promotedTime)) {
            $needsCpcPromotedAssets = $true
        }
    }

    if ($needsCpcPromotedAssets) {
        & $CpcAssetPromoterScript
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

if (Test-Path -LiteralPath $AmigaSfxPipeline) {
    & $AmigaSfxPipeline
    if (-not $?) {
        throw "Amiga-lydpipelinen feilet: $AmigaSfxPipeline"
    }
}

& $make -C $AmigaDir -j4 "program=$Program"
$makeExitCode = $LASTEXITCODE

if ($makeExitCode -eq 0) {
    $Exe2Adf = Join-Path $bin "exe2adf.exe"
    $BuiltExe = Join-Path $AmigaDir "$Program.exe"
    $AdfPath = Join-Path $AmigaDir "$Program.adf"
    if ((Test-Path -LiteralPath $Exe2Adf) -and (Test-Path -LiteralPath $BuiltExe)) {
        & $Exe2Adf -i $BuiltExe -l "Harrier Attack" -a $AdfPath
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "exe2adf feilet (kode $LASTEXITCODE) - ADF ble ikke generert."
        }
    } else {
        Write-Warning "Fant ikke exe2adf.exe eller $BuiltExe - hopper over ADF-generering."
    }
}

exit $makeExitCode
