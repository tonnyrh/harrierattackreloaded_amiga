<#
.SYNOPSIS
Bygger Harrier Attack Reloaded med LOGGEN-instrumentering og starter WinAPE.

.DESCRIPTION
Bygger cartridge-utgaven med -DLOGGEN=1 for terreng/mål R-bane-kalibrering
(Sprint 14.101) - logger per generert landkolonne hvilken beslutningsbane
(flat/bakke opp/bakke ned/måltype/blokkert) som kjørte, og R umiddelbart
før/etter den banen, slik at Amiga-portens CPC_R_COST_* konstanter kan
kontrolleres mot ekte M1-tellinger i stedet for antatte relative verdier.
Se HarrierAttackSourceNew2_alt_CRTC_CART16.asm (endofdata) for record-format.

.EXAMPLE
.\build-loggen.ps1
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = $PSScriptRoot
$compileDir = Join-Path $repoRoot 'compile'
$buildDir = Join-Path $compileDir 'build'
$toolsDir = Join-Path $repoRoot '.tools'
$rasmExe = Join-Path $toolsDir 'rasm\rasm.exe'
$cprFile = Join-Path $buildDir 'HarrierAttackReloaded.cpr'
$romFile = Join-Path $buildDir 'HarrierAttackReloadedROM.bin'
$symbolFile = Join-Path $buildDir 'HarrierAttackReloaded.sym'

function Get-WinApeExe {
    $winApeDir = Join-Path $toolsDir 'winape'
    $candidate = Get-ChildItem -LiteralPath $winApeDir -Filter 'WinAPE.exe' -File -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $candidate) { return $null }
    return $candidate.FullName
}

function Invoke-Native([string] $Executable, [string[]] $Arguments) {
    Write-Host "  > $([IO.Path]::GetFileName($Executable)) $($Arguments -join ' ')"
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable avsluttet med kode $LASTEXITCODE." }
}

function Add-RomBank([byte[]] $Rom, [int] $Bank, [string] $Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -gt 16384) {
        throw "$([IO.Path]::GetFileName($Path)) er $($bytes.Length) byte og overskrider 16 KiB."
    }
    [Array]::Copy($bytes, 0, $Rom, ($Bank * 16384), $bytes.Length)
}

function Write-Cpr([byte[]] $Rom, [string] $Path) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Create, [IO.FileAccess]::Write)
    $writer = [IO.BinaryWriter]::new($stream, [Text.Encoding]::ASCII, $false)
    try {
        $writer.Write([Text.Encoding]::ASCII.GetBytes('RIFF'))
        $writer.Write([uint32](4 + (4 * (8 + 16384))))
        $writer.Write([Text.Encoding]::ASCII.GetBytes('AMS!'))
        for ($bank = 0; $bank -lt 4; $bank++) {
            $writer.Write([Text.Encoding]::ASCII.GetBytes(('cb{0:D2}' -f $bank)))
            $writer.Write([uint32]16384)
            $writer.Write($Rom, $bank * 16384, 16384)
        }
    }
    finally { $writer.Dispose() }
}

function Merge-Symbols {
    $inputs = @('HARRIER1.sym', 'HARRIER2.sym', 'HARRSCR.sym', 'boot.sym') |
        ForEach-Object { Join-Path $compileDir $_ } |
        Where-Object { Test-Path -LiteralPath $_ }
    if ($inputs.Count -eq 0) { throw 'RASM produserte ingen symbolfiler.' }
    $seen = @{}
    $lines = foreach ($input in $inputs) {
        foreach ($line in [IO.File]::ReadAllLines($input)) {
            $trimmed = $line.Trim()
            if ($trimmed -and -not $seen.ContainsKey($trimmed)) {
                $seen[$trimmed] = $true
                $trimmed
            }
        }
    }
    [IO.File]::WriteAllLines($symbolFile, [string[]]$lines, [Text.Encoding]::ASCII)
}

function Set-IniSetting([string] $Path, [string] $Section, [string] $Key, [string] $Value) {
    $lines = [Collections.Generic.List[string]]::new()
    if (Test-Path -LiteralPath $Path) {
        foreach ($line in [IO.File]::ReadAllLines($Path)) { $lines.Add($line) }
    }
    $sectionHeader = "[$Section]"
    $sectionIndex = $lines.FindIndex([Predicate[string]]{ param($l) $l -ieq $sectionHeader })
    if ($sectionIndex -lt 0) {
        if ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -ne '') { $lines.Add('') }
        $lines.Add($sectionHeader)
        $lines.Add("$Key=$Value")
    } else {
        $nextSection = $lines.Count
        for ($i = $sectionIndex + 1; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match '^\[.+\]$') { $nextSection = $i; break }
        }
        $keyIndex = -1
        for ($i = $sectionIndex + 1; $i -lt $nextSection; $i++) {
            if ($lines[$i] -match "^$([regex]::Escape($Key))=") { $keyIndex = $i; break }
        }
        if ($keyIndex -ge 0) { $lines[$keyIndex] = "$Key=$Value" }
        else { $lines.Insert($nextSection, "$Key=$Value") }
    }
    [IO.File]::WriteAllLines($Path, $lines, [Text.Encoding]::ASCII)
}

function Enable-WinApeCartridge([string] $WinApeExe) {
    $winApeDir = Split-Path -Parent $WinApeExe
    $romDir = Join-Path $winApeDir 'ROM'
    $installedCartridge = Join-Path $romDir 'HarrierAttackReloaded.cpr'
    $iniFile = Join-Path $winApeDir 'WinAPE.ini'
    New-Item -ItemType Directory -Path $romDir -Force | Out-Null
    Copy-Item -LiteralPath $cprFile -Destination $installedCartridge -Force
    Set-IniSetting $iniFile 'ROMS' 'Cartridge' 'HarrierAttackReloaded.cpr'
    Set-IniSetting $iniFile 'ROMS' 'Cartridge Enabled' 'true'
    Set-IniSetting $iniFile 'ROMS' 'Lower' ''
    for ($slot = 0; $slot -le 15; $slot++) {
        Set-IniSetting $iniFile 'ROMS' "Upper($slot)" ''
    }
    Set-IniSetting $iniFile 'Configuration' 'Enable Plus' 'true'
    Set-IniSetting $iniFile 'Configuration' 'Plus PPI' 'true'
    Set-IniSetting $iniFile 'Configuration' 'CRTC Type' '3'
}

# ---- Build ----
Write-Host 'Bygger LOGGEN-cartridge ...' -ForegroundColor Cyan
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

$fontSource = Join-Path $repoRoot 'AMSTRADFONT3.asm'
$compatibleFontSource = Join-Path $compileDir '.AMSTRADFONT3.rasm.asm'
$fontText = [IO.File]::ReadAllText($fontSource)
$fontText = $fontText.Replace('read "CPSoundEffectGenerator2.asm"', 'include "../CPSoundEffectGenerator2.asm"')
[IO.File]::WriteAllText($compatibleFontSource, $fontText, [Text.Encoding]::ASCII)

Push-Location $compileDir
try {
    Invoke-Native $rasmExe @('-I..', '-DLOGGEN=1', '-DISCART=1', '-amper', '-sw', '-sa', '../HarrierAttackSourceNew2_alt_CRTC_CART16.asm', 'HARRIER1')
    Invoke-Native $rasmExe @('-I..', '-DHARRIERATTACK=1', '-amper', '-sw', '-sa', '.AMSTRADFONT3.rasm.asm', 'HARRIER2')
    Invoke-Native $rasmExe @('-I..', '-amper', '-sw', '-sa', '../HARR_SCR2.asm', 'HARRSCR')
    Invoke-Native $rasmExe @('-I..', '-amper', '-sw', '-sa', '../boot2.asm', 'boot')
}
finally {
    Pop-Location
    if (Test-Path -LiteralPath $compatibleFontSource) { Remove-Item -LiteralPath $compatibleFontSource -Force }
}

$rom = [byte[]]::new(65536)
Add-RomBank $rom 0 (Join-Path $compileDir 'boot.bin')
Add-RomBank $rom 1 (Join-Path $compileDir 'HARRSCR.bin')
Add-RomBank $rom 2 (Join-Path $compileDir 'HARRIER1.bin')
Add-RomBank $rom 3 (Join-Path $compileDir 'HARRIER2.bin')
[IO.File]::WriteAllBytes($romFile, $rom)
Write-Cpr $rom $cprFile
Merge-Symbols
Write-Host "Ferdig: $cprFile" -ForegroundColor Green

# ---- Start WinAPE ----
$winApeExe = Get-WinApeExe
if ($null -ne $winApeExe) {
    Enable-WinApeCartridge $winApeExe
    Write-Host 'Starter WinAPE med LOGGEN-cartridge ...' -ForegroundColor Cyan
    Write-Host 'Spill til land-seksjonen, apne debugger (F7), og kjor:' -ForegroundColor Yellow
    Write-Host '  SAVE "cpc_log.bin",&F000,2000' -ForegroundColor Yellow
    Start-Process -FilePath $winApeExe -WorkingDirectory (Split-Path -Parent $winApeExe) -ArgumentList @("/SYM:`"$symbolFile`"")
} else {
    Write-Host 'WinAPE ikke funnet. CPR er klar i compile\build\' -ForegroundColor Yellow
}
