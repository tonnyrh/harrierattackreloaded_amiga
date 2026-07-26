#Requires -Version 5.1
[CmdletBinding()]
param(
    [switch]$NoBuild,
    [switch]$NoZip,
    [string]$OutputRoot = "dist\amiga",
    [string]$PackageName = "HarrierAttackReloaded-Amiga-A500-playtest"
)

$ErrorActionPreference = "Stop"

$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$OutputRootPath = if ([System.IO.Path]::IsPathRooted($OutputRoot)) {
    $OutputRoot
} else {
    Join-Path $Root $OutputRoot
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$packageBaseName = "$PackageName-$timestamp"
$stagingDir = Join-Path $OutputRootPath $packageBaseName
$debugDir = Join-Path $stagingDir "debug"

$buildScript = Join-Path $Root "amiga-build.ps1"
$amigaDir = Join-Path $Root "amiga"
$exePath = Join-Path $amigaDir "out\harrier_amiga.exe"
$elfPath = Join-Path $amigaDir "out\harrier_amiga.elf"
$mapPath = Join-Path $amigaDir "out\harrier_amiga.map"
$asmPath = Join-Path $amigaDir "out\harrier_amiga.s"
$playtestReadme = Join-Path $amigaDir "README_PLAYTEST.md"
$sfxReadme = Join-Path $amigaDir "assets\sfx\README.md"
$portPlan = Join-Path $Root "AMIGA_PORT_PLAN.md"

if (-not $NoBuild) {
    if (-not (Test-Path -LiteralPath $buildScript)) {
        throw "Fant ikke build-script: $buildScript"
    }
    & $buildScript
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (-not (Test-Path -LiteralPath $exePath)) {
    throw "Fant ikke bygget Amiga executable: $exePath"
}

New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null
New-Item -ItemType Directory -Force -Path $debugDir | Out-Null

Copy-Item -LiteralPath $exePath -Destination (Join-Path $stagingDir "harrier_amiga.exe") -Force

foreach ($debugFile in @($elfPath, $mapPath, $asmPath)) {
    if (Test-Path -LiteralPath $debugFile) {
        Copy-Item -LiteralPath $debugFile -Destination $debugDir -Force
    }
}

if (Test-Path -LiteralPath $playtestReadme) {
    Copy-Item -LiteralPath $playtestReadme -Destination (Join-Path $stagingDir "README_PLAYTEST.md") -Force
}
if (Test-Path -LiteralPath $sfxReadme) {
    Copy-Item -LiteralPath $sfxReadme -Destination (Join-Path $stagingDir "SFX_README.md") -Force
}
if (Test-Path -LiteralPath $portPlan) {
    Copy-Item -LiteralPath $portPlan -Destination (Join-Path $stagingDir "AMIGA_PORT_PLAN.md") -Force
}

$gitCommit = "unknown"
try {
    $gitCommit = (& git -C $Root rev-parse --short HEAD 2>$null)
    if (-not $gitCommit) {
        $gitCommit = "unknown"
    }
} catch {
    $gitCommit = "unknown"
}

$exeHash = Get-FileHash -Algorithm SHA256 -LiteralPath $exePath
$exeInfo = Get-Item -LiteralPath $exePath
$versionText = @(
    "Harrier Attack Reloaded - Amiga A500 playtest package",
    "Created: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')",
    "Source: $Root",
    "Git commit: $gitCommit",
    "Target: Amiga 500, OCS, PAL, Kickstart 1.3, 68000, 512 KiB chip + 512 KiB slow RAM",
    "Kickstart ROM: not included",
    "",
    "Executable: harrier_amiga.exe",
    "Size: $($exeInfo.Length) bytes",
    "SHA256: $($exeHash.Hash)",
    "",
    "Build command:",
    ".\amiga-build.ps1",
    "",
    "Package command:",
    ".\package-amiga.ps1",
    "",
    "Run/debug from repository:",
    "Open VS Code and press F5 using 'Amiga 500 debug (KS1.3, 1MB)'."
)
Set-Content -LiteralPath (Join-Path $stagingDir "VERSION.txt") -Value $versionText -Encoding UTF8

if (-not $NoZip) {
    $zipPath = Join-Path $OutputRootPath "$packageBaseName.zip"
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Compress-Archive -Path (Join-Path $stagingDir "*") -DestinationPath $zipPath -Force
    Write-Host "Pakket zip: $zipPath"
}

Write-Host "Pakket mappe: $stagingDir"
Write-Host "Amiga executable: $(Join-Path $stagingDir 'harrier_amiga.exe')"
