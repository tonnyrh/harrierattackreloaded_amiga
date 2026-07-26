<#
.SYNOPSIS
Installerer verktoy, bygger Harrier Attack Reloaded og starter WinAPE.

.DESCRIPTION
Alt installeres lokalt i .tools; administratorrettigheter er ikke nodvendig.
Cartridge-utgaven bygges fordi den kan produseres pa Windows uten Bash,
RomInject, BuildCPR eller 2CDT. RASM-symboler lastes inn i WinAPE-debuggeren.

.PARAMETER Action
Validate, Install, Build, Run, Debug eller All (standard).

.EXAMPLE
.\setup-dev.ps1 -Action All

.EXAMPLE
.\setup-dev.ps1 -Action Build

.EXAMPLE
.\setup-dev.ps1 -Action Debug
#>
[CmdletBinding()]
param(
    [ValidateSet('Validate', 'Install', 'Build', 'Run', 'Debug', 'All')]
    [string] $Action = 'All'
)

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

function Test-IsWindows {
    return [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform(
        [System.Runtime.InteropServices.OSPlatform]::Windows
    )
}

function Get-WinApeExe {
    $winApeDir = Join-Path $toolsDir 'winape'
    $candidate = Get-ChildItem -LiteralPath $winApeDir -Filter 'WinAPE.exe' -File -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $candidate) { return $null }
    return $candidate.FullName
}

function Assert-File([string] $Path, [string] $Description) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description mangler: $Path"
    }
}

function Invoke-Native([string] $Executable, [string[]] $Arguments) {
    Write-Host "  > $([IO.Path]::GetFileName($Executable)) $($Arguments -join ' ')"
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable avsluttet med kode $LASTEXITCODE."
    }
}

function Install-Tools {
    Write-Host 'Installerer utviklingsverktoy lokalt i .tools ...' -ForegroundColor Cyan
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    New-Item -ItemType Directory -Path (Split-Path -Parent $rasmExe) -Force | Out-Null

    $headers = @{ 'User-Agent' = 'HarrierAttackReloaded-setup' }
    $release = Invoke-RestMethod -Uri 'https://api.github.com/repos/EdouardBERGE/rasm/releases/latest' -Headers $headers
    $asset = $release.assets | Where-Object name -EQ 'rasm_w64.exe' | Select-Object -First 1
    if ($null -eq $asset) {
        $asset = $release.assets | Where-Object name -EQ 'rasm_w32.exe' | Select-Object -First 1
    }
    if ($null -eq $asset) { throw 'Fant ingen Windows-utgave av RASM i siste GitHub-release.' }
    Invoke-WebRequest -Uri $asset.browser_download_url -Headers $headers -OutFile $rasmExe -UseBasicParsing

    $winApeDir = Join-Path $toolsDir 'winape'
    $winApeZip = Join-Path $toolsDir 'WinAPE20B2.zip'
    New-Item -ItemType Directory -Path $winApeDir -Force | Out-Null
    try {
        Invoke-WebRequest -Uri 'https://www.winape.net/download/WinAPE20B2.zip' -OutFile $winApeZip -UseBasicParsing
        Expand-Archive -LiteralPath $winApeZip -DestinationPath $winApeDir -Force
    }
    finally {
        if (Test-Path -LiteralPath $winApeZip) { Remove-Item -LiteralPath $winApeZip -Force }
    }

    Assert-File $rasmExe 'RASM'
    $winApeExe = Get-WinApeExe
    if ($null -eq $winApeExe) { throw "WinAPE.exe ble ikke funnet under $winApeDir." }
    Write-Host "RASM $($release.tag_name) og WinAPE 2.0b2 er klare." -ForegroundColor Green
}

function Test-Environment([switch] $RequireBuild) {
    if (-not (Test-IsWindows)) { throw 'Dette oppsettet krever Windows.' }
    if ($PSVersionTable.PSVersion -lt [Version]'5.1') { throw 'PowerShell 5.1 eller nyere kreves.' }
    Assert-File $rasmExe 'RASM (kjor -Action Install forst)'
    if ($null -eq (Get-WinApeExe)) { throw 'WinAPE mangler (kjor -Action Install forst).' }
    if ($RequireBuild) { Assert-File $cprFile 'Bygd spill' }
    Write-Host 'Utviklingsmiljoet er gyldig.' -ForegroundColor Green
}

function Add-RomBank([byte[]] $Rom, [int] $Bank, [string] $Path) {
    Assert-File $Path "ROM-bank $Bank"
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -gt 16384) {
        throw "$([IO.Path]::GetFileName($Path)) er $($bytes.Length) byte og overskrider en 16 KiB-bank."
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
    finally {
        $writer.Dispose()
    }
}

function Merge-Symbols {
    $inputs = @('HARRIER1.sym', 'HARRIER2.sym', 'HARRSCR.sym', 'boot.sym') |
        ForEach-Object { Join-Path $compileDir $_ } |
        Where-Object { Test-Path -LiteralPath $_ }
    if ($inputs.Count -eq 0) { throw 'RASM produserte ingen WinAPE-symbolfiler.' }

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
    $sectionIndex = $lines.FindIndex([Predicate[string]]{ param($line) $line -ieq $sectionHeader })
    if ($sectionIndex -lt 0) {
        if ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -ne '') { $lines.Add('') }
        $lines.Add($sectionHeader)
        $lines.Add("$Key=$Value")
    }
    else {
        $nextSection = $lines.Count
        for ($index = $sectionIndex + 1; $index -lt $lines.Count; $index++) {
            if ($lines[$index] -match '^\[.+\]$') { $nextSection = $index; break }
        }
        $keyIndex = -1
        for ($index = $sectionIndex + 1; $index -lt $nextSection; $index++) {
            if ($lines[$index] -match "^$([regex]::Escape($Key))=") { $keyIndex = $index; break }
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

    # WinAPEs kommandolinje behandler et filnavn som en disk, ikke en cartridge.
    # Cartridge og Plus-modus ma derfor velges i den lokale WinAPE-konfigurasjonen.
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

function Build-Game {
    Test-Environment
    Write-Host 'Bygger cartridge-utgaven ...' -ForegroundColor Cyan
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
    $fontSource = Join-Path $repoRoot 'AMSTRADFONT3.asm'
    $compatibleFontSource = Join-Path $compileDir '.AMSTRADFONT3.rasm.asm'
    $fontText = [IO.File]::ReadAllText($fontSource)
    if (-not $fontText.Contains('read "CPSoundEffectGenerator2.asm"')) {
        throw 'Fant ikke forventet READ-direktiv i AMSTRADFONT3.asm.'
    }
    # Den gamle kilden bruker WinAPE-direktivet READ; moderne RASM bruker INCLUDE.
    $fontText = $fontText.Replace('read "CPSoundEffectGenerator2.asm"', 'include "../CPSoundEffectGenerator2.asm"')
    [IO.File]::WriteAllText($compatibleFontSource, $fontText, [Text.Encoding]::ASCII)
    Push-Location $compileDir
    try {
        Invoke-Native $rasmExe @('-I..', '-DISCART=1', '-amper', '-sw', '-sa', '../HarrierAttackSourceNew2_alt_CRTC_CART16.asm', 'HARRIER1')
        Invoke-Native $rasmExe @('-I..', '-DHARRIERATTACK=1', '-amper', '-sw', '-sa', '.AMSTRADFONT3.rasm.asm', 'HARRIER2')
        Invoke-Native $rasmExe @('-I..', '-amper', '-sw', '-sa', '../HARR_SCR2.asm', 'HARRSCR')
        Invoke-Native $rasmExe @('-I..', '-amper', '-sw', '-sa', '../boot2.asm', 'boot')
    }
    finally {
        Pop-Location
        if (Test-Path -LiteralPath $compatibleFontSource) {
            Remove-Item -LiteralPath $compatibleFontSource -Force
        }
    }

    $rom = [byte[]]::new(65536)
    Add-RomBank $rom 0 (Join-Path $compileDir 'boot.bin')
    Add-RomBank $rom 1 (Join-Path $compileDir 'HARRSCR.bin')
    Add-RomBank $rom 2 (Join-Path $compileDir 'HARRIER1.bin')
    Add-RomBank $rom 3 (Join-Path $compileDir 'HARRIER2.bin')
    [IO.File]::WriteAllBytes($romFile, $rom)
    Write-Cpr $rom $cprFile
    Merge-Symbols
    Assert-File $cprFile 'CPR-resultat'
    Write-Host "Ferdig: $cprFile" -ForegroundColor Green
}

function Start-Game([switch] $DebugMode) {
    Build-Game
    $winApeExe = Get-WinApeExe
    Enable-WinApeCartridge $winApeExe
    if ($DebugMode) {
        Write-Host 'WinAPE starter med symboler. F7 apner debuggeren; F3 apner assembleren.' -ForegroundColor Yellow
        Write-Host 'Sett breakpoints pa navnene fra HarrierAttackReloaded.sym i debuggeren.' -ForegroundColor Yellow
    }
    else {
        Write-Host 'Starter Harrier Attack Reloaded i WinAPE ...' -ForegroundColor Cyan
    }
    Start-Process -FilePath $winApeExe -WorkingDirectory (Split-Path -Parent $winApeExe) -ArgumentList @("/SYM:`"$symbolFile`"")
}

switch ($Action) {
    'Validate' { Test-Environment -RequireBuild }
    'Install'  { Install-Tools; Test-Environment }
    'Build'    { Build-Game }
    'Run'      { Start-Game }
    'Debug'    { Start-Game -DebugMode }
    'All'      { Install-Tools; Start-Game -DebugMode }
}
