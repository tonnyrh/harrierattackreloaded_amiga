#Requires -Version 5.1
[CmdletBinding()]
param(
    [switch]$SkipCompile,
    [switch]$NormalOnly,
    [switch]$TurboOnly,
    [string]$TwoCdtPath
)

$ErrorActionPreference = "Stop"

function Ensure-Directory {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Resolve-Tool {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string[]]$Candidates,
        [string]$Hint
    )

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $fromPath = Get-Command $Name -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    if ($Hint) {
        throw "Fant ikke $Name. $Hint"
    }

    throw "Fant ikke $Name."
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [string]$WorkingDirectory
    )

    $oldLocation = Get-Location
    try {
        if ($WorkingDirectory) {
            Set-Location -LiteralPath $WorkingDirectory
        }
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$FilePath feilet med exitkode $LASTEXITCODE"
        }
    } finally {
        Set-Location $oldLocation
    }
}

$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$CompileDir = Join-Path $Root "compile"
$BuildDir = Join-Path $CompileDir "build"
$Rasm = Resolve-Tool -Name "rasm.exe" -Candidates @(
    (Join-Path $CompileDir "rasm.exe"),
    (Join-Path $Root ".tools\rasm\rasm.exe")
) -Hint "Kjor .\setup-dev-amstrad.ps1 forst."

$TwoCdtCandidates = @()
if ($TwoCdtPath) {
    $TwoCdtCandidates += $TwoCdtPath
}
$TwoCdtCandidates += @(
    (Join-Path $CompileDir "2cdt.exe"),
    (Join-Path $Root ".tools\2cdt\2cdt.exe"),
    (Join-Path $Root ".tools\2cdt\2cdt")
)

$TwoCdt = Resolve-Tool -Name "2cdt.exe" -Candidates $TwoCdtCandidates -Hint @"
Last ned 2CDT og legg 2cdt.exe i:
  $Root\.tools\2cdt\2cdt.exe

Kjent upstream er 2CDT fra Kevin Thacker / CPCWiki:
  http://cpctech.cpcwiki.de/download/2cdt.zip

Alternativt pek direkte:
  .\make-cdt.ps1 -TwoCdtPath C:\path\to\2cdt.exe
"@

if ($NormalOnly -and $TurboOnly) {
    throw "Velg enten -NormalOnly eller -TurboOnly, ikke begge."
}

if (-not (Test-Path -LiteralPath $CompileDir)) {
    throw "Fant ikke compile-mappen: $CompileDir"
}

Ensure-Directory -Path $BuildDir

if (-not $SkipCompile) {
    Write-Host "Assembler cassette-binærfiler med RASM..."
    Invoke-Checked -FilePath $Rasm -WorkingDirectory $CompileDir -Arguments @(
        "../HarrierAttackSourceNew2_alt_CRTC_CART16.asm", "HARRIER1", "-I..", "-DISCART=1", "-DHARRIERATTACK=1", "-DISCASSETTE=1", "-amper", "-sw", "-sa"
    )
    Invoke-Checked -FilePath $Rasm -WorkingDirectory $CompileDir -Arguments @(
        "../AMSTRADFONT3.asm", "HARRIER2", "-I..", "-DHARRIERATTACK=1", "-amper", "-sw", "-sa"
    )
    Invoke-Checked -FilePath $Rasm -WorkingDirectory $CompileDir -Arguments @(
        "../HARR_SCR2.asm", "HARRSCR", "-I..", "-amper", "-sw", "-sa"
    )
    Invoke-Checked -FilePath $Rasm -WorkingDirectory $CompileDir -Arguments @(
        "../loadercasette.asm", "LOADERC", "-I..", "-amper", "-sw", "-sa"
    )
}

$RequiredBins = @("LOADERC.bin", "HARRSCR.bin", "HARRIER1.bin", "HARRIER2.bin")
foreach ($bin in $RequiredBins) {
    $path = Join-Path $CompileDir $bin
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Mangler $path. Kjor uten -SkipCompile for a bygge cassette-binærfilene."
    }
}

if (-not $NormalOnly) {
    $TurboCdt = Join-Path $BuildDir "HarrierAttackReloaded.cdt"
    if (Test-Path -LiteralPath $TurboCdt) {
        Remove-Item -LiteralPath $TurboCdt -Force
    }

    Write-Host "Lager turbo CDT: $TurboCdt"
    Invoke-Checked -FilePath $TwoCdt -WorkingDirectory $CompileDir -Arguments @("-n", "-r", "harrier.bin", ".\LOADERC.bin", $TurboCdt)
    Invoke-Checked -FilePath $TwoCdt -WorkingDirectory $CompileDir -Arguments @("-r", "harrscr.bin", ".\HARRSCR.bin", $TurboCdt)
    Invoke-Checked -FilePath $TwoCdt -WorkingDirectory $CompileDir -Arguments @("-r", "harrier1.bin", ".\HARRIER1.bin", $TurboCdt)
    Invoke-Checked -FilePath $TwoCdt -WorkingDirectory $CompileDir -Arguments @("-r", "harrier2.bin", ".\HARRIER2.bin", $TurboCdt)
}

if (-not $TurboOnly) {
    $NormalCdt = Join-Path $BuildDir "HarrierAttackReloadedN.cdt"
    if (Test-Path -LiteralPath $NormalCdt) {
        Remove-Item -LiteralPath $NormalCdt -Force
    }

    Write-Host "Lager normal-baud CDT for ekte hardware: $NormalCdt"
    Invoke-Checked -FilePath $TwoCdt -WorkingDirectory $CompileDir -Arguments @("-t", "0", "-s", "0", "-n", "-r", "harrier.bin", ".\LOADERC.bin", $NormalCdt)
    Invoke-Checked -FilePath $TwoCdt -WorkingDirectory $CompileDir -Arguments @("-t", "0", "-s", "0", "-r", "harrscr.bin", ".\HARRSCR.bin", $NormalCdt)
    Invoke-Checked -FilePath $TwoCdt -WorkingDirectory $CompileDir -Arguments @("-t", "0", "-s", "0", "-r", "harrier1.bin", ".\HARRIER1.bin", $NormalCdt)
    Invoke-Checked -FilePath $TwoCdt -WorkingDirectory $CompileDir -Arguments @("-t", "0", "-s", "0", "-r", "harrier2.bin", ".\HARRIER2.bin", $NormalCdt)
}

Write-Host "Ferdig."
Get-ChildItem -LiteralPath $BuildDir -Filter "*.cdt" | Select-Object FullName, Length, LastWriteTime
