#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$Version = "0.9.0-beta.1",
    [switch]$NoBuild,
    [switch]$NoZip,
    [switch]$IncludeDebug,
    [string]$OutputRoot = "dist\release"
)

$ErrorActionPreference = "Stop"

$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$OutputRootPath = if ([System.IO.Path]::IsPathRooted($OutputRoot)) {
    $OutputRoot
} else {
    Join-Path $Root $OutputRoot
}

$normalizedVersion = $Version.Trim()
if ($normalizedVersion.StartsWith("v", [System.StringComparison]::OrdinalIgnoreCase)) {
    $normalizedVersion = $normalizedVersion.Substring(1)
}
if ($normalizedVersion -notmatch '^[0-9A-Za-z][0-9A-Za-z.-]*$') {
    throw "Ugyldig releaseversjon: $Version"
}

$tagName = "v$normalizedVersion"
$assetBaseName = "HarrierAttackReloaded-Amiga-$tagName"
$hdPackageName = "$assetBaseName-HD"
$stagingDir = Join-Path $OutputRootPath $hdPackageName
$zipPath = Join-Path $OutputRootPath "$hdPackageName.zip"
$releaseAdfPath = Join-Path $OutputRootPath "$assetBaseName.adf"
$checksumPath = Join-Path $OutputRootPath "$assetBaseName-SHA256SUMS.txt"
$debugZipPath = Join-Path $OutputRootPath "$assetBaseName-debug-symbols.zip"

$buildScript = Join-Path $Root "amiga-build.ps1"
$amigaDir = Join-Path $Root "amiga"
$outDir = Join-Path $amigaDir "out"
$exePath = Join-Path $outDir "harrier_amiga.exe"
$iconPath = Join-Path $outDir "harrier_amiga.exe.info"
$loadingScreenPath = Join-Path $outDir "loading_screen.bpl"
$adfPath = Join-Path $outDir "harrier_amiga.adf"
$elfPath = Join-Path $outDir "harrier_amiga.elf"
$mapPath = Join-Path $outDir "harrier_amiga.map"
$publicReadme = Join-Path $amigaDir "README_PUBLIC_BETA.txt"

if (-not $NoBuild) {
    if (-not (Test-Path -LiteralPath $buildScript)) {
        throw "Fant ikke build-script: $buildScript"
    }
    & $buildScript
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

foreach ($requiredFile in @(
    $exePath,
    $iconPath,
    $loadingScreenPath,
    $adfPath,
    $publicReadme
)) {
    if (-not (Test-Path -LiteralPath $requiredFile)) {
        throw "Mangler releasefil: $requiredFile"
    }
}

New-Item -ItemType Directory -Force -Path $OutputRootPath | Out-Null
if (Test-Path -LiteralPath $stagingDir) {
    Remove-Item -LiteralPath $stagingDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null

Copy-Item -LiteralPath $exePath -Destination (Join-Path $stagingDir "harrier_amiga.exe") -Force
Copy-Item -LiteralPath $iconPath -Destination (Join-Path $stagingDir "harrier_amiga.exe.info") -Force
Copy-Item -LiteralPath $loadingScreenPath -Destination (Join-Path $stagingDir "loading_screen.bpl") -Force
Copy-Item -LiteralPath $publicReadme -Destination (Join-Path $stagingDir "README.txt") -Force
Copy-Item -LiteralPath $adfPath -Destination $releaseAdfPath -Force

$gitCommit = "unknown"
try {
    $gitCommit = (& git -C $Root rev-parse --short HEAD 2>$null)
    if (-not $gitCommit) {
        $gitCommit = "unknown"
    }
} catch {
    $gitCommit = "unknown"
}

$versionText = @(
    "Harrier Attack Reloaded - Amiga Public Beta",
    "Version: $tagName",
    "Git commit: $gitCommit",
    "Created: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')",
    "",
    "Target: stock PAL Amiga 500, OCS, 68000",
    "Memory: 512 KiB chip RAM + 512 KiB expansion RAM",
    "Kickstart ROM: not included",
    "",
    "Hard-disk files:",
    "  harrier_amiga.exe",
    "  harrier_amiga.exe.info",
    "  loading_screen.bpl",
    "",
    "Source repository:",
    "https://github.com/tonnyrh/harrierattackreloaded_amiga"
)
Set-Content -LiteralPath (Join-Path $stagingDir "VERSION.txt") `
    -Value $versionText -Encoding ASCII

if (-not $NoZip) {
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Compress-Archive -Path (Join-Path $stagingDir "*") -DestinationPath $zipPath -Force
}

$releaseAssets = @($releaseAdfPath)
if (-not $NoZip) {
    $releaseAssets += $zipPath
}

if ($IncludeDebug) {
    $debugStagingDir = Join-Path $OutputRootPath "$assetBaseName-debug-symbols"
    if (Test-Path -LiteralPath $debugStagingDir) {
        Remove-Item -LiteralPath $debugStagingDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $debugStagingDir | Out-Null
    foreach ($debugFile in @($elfPath, $mapPath)) {
        if (Test-Path -LiteralPath $debugFile) {
            Copy-Item -LiteralPath $debugFile -Destination $debugStagingDir -Force
        }
    }
    if (Test-Path -LiteralPath $debugZipPath) {
        Remove-Item -LiteralPath $debugZipPath -Force
    }
    Compress-Archive -Path (Join-Path $debugStagingDir "*") `
        -DestinationPath $debugZipPath -Force
    $releaseAssets += $debugZipPath
}

$checksumLines = foreach ($assetPath in $releaseAssets) {
    $hash = Get-FileHash -Algorithm SHA256 -LiteralPath $assetPath
    "{0}  {1}" -f $hash.Hash.ToLowerInvariant(), (Split-Path -Leaf $assetPath)
}
Set-Content -LiteralPath $checksumPath -Value $checksumLines -Encoding ASCII

Write-Host "Public beta-pakke er klar:"
Write-Host "  ADF:       $releaseAdfPath"
if (-not $NoZip) {
    Write-Host "  HD ZIP:    $zipPath"
}
if ($IncludeDebug) {
    Write-Host "  Debug ZIP: $debugZipPath"
}
Write-Host "  SHA256:    $checksumPath"
Write-Host "  Tag:       $tagName"
