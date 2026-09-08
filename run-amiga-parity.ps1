#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet('A500', 'A1200')]
    [string]$Machine = 'A500',
    [switch]$Visible,
    [int[]]$Skills = @(1, 3, 5),
    [ValidateRange(1, 15)]
    [int]$CruiseSpeed = 15,
    [ValidateRange(0, 2)]
    [int]$WingmanControl = 1,
    [ValidateRange(1, 65535)]
    [int]$SessionSeed = 12040,
    [int[]]$EnemyPlaneRates = @(1),
    [switch]$EnemyPlaneExercise,
    [switch]$WingmanFormationExercise,
    [switch]$WeaponStress,
    [string]$ExtraCcFlags = "",
    [ValidatePattern('^[A-Za-z0-9_-]*$')]
    [string]$ResultTag = "",
    [ValidateRange(30, 600)]
    [int]$TimeoutSeconds = 480
)

$ErrorActionPreference = "Stop"
$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$AmigaDir = Join-Path $Root "amiga"
$OutDir = Join-Path $AmigaDir "out"
$ResultDir = Join-Path $Root ".tmp\amiga-parity-results"
$Config = Join-Path $AmigaDir "harrier_headless.uae"
$ExtensionRoot = Join-Path $env:USERPROFILE ".vscode\extensions"
$Extension = Get-ChildItem -Directory $ExtensionRoot -Filter "bartmanabyss.amiga-debug-*" -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    Select-Object -First 1

if (-not $Extension) {
    throw "Fant ikke Bartman/Abyss Amiga-utvidelsen under $ExtensionRoot."
}

$Bin = Join-Path $Extension.FullName "bin\win32"
$Make = Join-Path $Bin "gnumake.exe"
$WinUae = Join-Path $Bin "winuae-gdb.exe"
foreach ($required in @($Make, $WinUae, $Config)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Mangler nodvendig fil: $required"
    }
}

# A cycle-exact A500 run can remain quiet for several minutes. Refuse to
# launch a second emulator against the same writable DH1: directory; two
# concurrent runs race on the CSV files and make a healthy run look hung.
$existingWinUae = Get-Process -Name "winuae-gdb" -ErrorAction SilentlyContinue
if ($existingWinUae) {
    $ids = ($existingWinUae | ForEach-Object { $_.Id }) -join ", "
    throw "WinUAE headless kjorer allerede (PID $ids). Vent til testen er ferdig eller avslutt den for en ny maaling."
}

$env:PATH = "$env:PATH;$Bin\opt\bin;$Bin"
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
New-Item -ItemType Directory -Path $ResultDir -Force | Out-Null

if ($Machine -eq 'A1200') {
    # Stock PAL A1200: 68EC020, AGA, 2 MiB chip, no expansion RAM/JIT.
    $rom = Join-Path $Root '.tools\Amiga\Kick\amiga-os-300-a1200.rom'
    if (-not (Test-Path -LiteralPath $rom)) { throw "Missing A1200 ROM: $rom" }
    $a1200Config = Get-Content -LiteralPath $Config | Where-Object {
        $_ -notmatch '^(quickstart|kickstart_rom_file|chipmem_size|bogomem_size|fastmem_size)='
    }
    $Config = Join-Path $ResultDir 'stock-a1200.uae'
    @('quickstart=a1200,0') + $a1200Config + @(
        "kickstart_rom_file=$rom", 'chipset=aga', 'chipset_compatible=A1200',
        'cpu_model=68020', 'cpu_24bit_addressing=true', 'cpu_compatible=true',
        'cpu_speed=real', 'cpu_multiplier=4', 'cachesize=0', 'fpu_model=0',
        'chipmem_size=4', 'fastmem_size=0', 'bogomem_size=0', 'z3mem_size=0'
    ) | Set-Content -LiteralPath $Config -Encoding ASCII
}

function Invoke-Make {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    & $Make @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "gnumake feilet med exitkode $LASTEXITCODE."
    }
}

try {
    foreach ($EnemyPlaneRate in $EnemyPlaneRates) {
        if ($EnemyPlaneRate -lt 1 -or $EnemyPlaneRate -gt 3) {
            throw "EnemyPlaneRate ma vare 1, 2 eller 3: $EnemyPlaneRate"
        }
        foreach ($Skill in $Skills) {
        if ($Skill -lt 1 -or $Skill -gt 5) {
            throw "Skill ma vare mellom 1 og 5: $Skill"
        }

        Write-Host "Bygger og maaler Amiga skill $Skill, enemy ${EnemyPlaneRate}x (cycle-exact $Machine)..."
        Invoke-Make -Arguments @("-C", $AmigaDir, "clean")
        foreach ($name in @("perf_log.csv", "land_log.csv", "parity_log.csv", "enemy_plane_log.csv")) {
            $path = Join-Path $OutDir $name
            if (Test-Path -LiteralPath $path) {
                Remove-Item -LiteralPath $path -Force
            }
        }

        $flags = "-DHAR_DEBUG_PERF_LOG=1 -DHAR_DEBUG_LAND_LOG=1 -DHAR_DEBUG_ENEMY_PLANE_LOG=1 -DHAR_ENEMY_PLANE_INTERPOLATION_PIXELS=$EnemyPlaneRate -DHAR_HEADLESS_AUTOPLAY=1 -DHAR_HEADLESS_SKILL_LEVEL=$Skill -DHAR_HEADLESS_CRUISE_SPEED=$CruiseSpeed -DHAR_HEADLESS_WINGMAN_CONTROL=$WingmanControl -DHAR_VALIDATION_SESSION_SEED=$SessionSeed"
        if ($EnemyPlaneExercise) {
            $flags = "$flags -DHAR_HEADLESS_ENEMY_PLANE_EXERCISE=1"
        }
        if ($WingmanFormationExercise) {
            if ($WingmanControl -ne 1) {
                throw "WingmanFormationExercise krever -WingmanControl 1."
            }
            $flags = "$flags -DHAR_HEADLESS_WINGMAN_FORMATION_EXERCISE=1"
        }
        if ($WeaponStress) {
            if ($WingmanControl -ne 2) {
                throw "WeaponStress krever -WingmanControl 2 slik at begge spillerne kan skyte kontinuerlig."
            }
            $flags = "$flags -DHAR_HEADLESS_WEAPON_STRESS=1"
        }
        if (-not [string]::IsNullOrWhiteSpace($ExtraCcFlags)) {
            $flags = "$flags $ExtraCcFlags"
        }
        Invoke-Make -Arguments @("-C", $AmigaDir, "-j4", "program=out/harrier_amiga", "EXTRA_CCFLAGS=$flags")

        $testWindowStyle = if ($Visible) { 'Normal' } else { 'Hidden' }
        $process = Start-Process -FilePath $WinUae -ArgumentList @("-f", $Config) -WindowStyle $testWindowStyle -PassThru
        Write-Host "WinUAE headless startet som PID $($process.Id). Venter pa fire CSV-resultater..."
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        $nextProgress = [DateTime]::UtcNow.AddSeconds(10)
        # parity_log is written slightly before the optional diagnostic logs.
        # Waiting only for that first file raced the Amiga shutdown and could
        # kill WinUAE before enemy_plane_log/land_log had been closed.
        $expectedLogs = @("perf_log.csv", "land_log.csv", "parity_log.csv", "enemy_plane_log.csv") |
            ForEach-Object { Join-Path $OutDir $_ }
        try {
            while (@($expectedLogs | Where-Object { -not (Test-Path -LiteralPath $_) }).Count -ne 0) {
                if ([DateTime]::UtcNow -ge $deadline) {
                    throw "Tidsavbrudd for skill $Skill etter $TimeoutSeconds sekunder."
                }
                if ($process.HasExited) {
                    throw "WinUAE avsluttet for skill $Skill uten parity_log.csv."
                }
                Start-Sleep -Milliseconds 500
                $process.Refresh()
                if ([DateTime]::UtcNow -ge $nextProgress) {
                    $ready = @($expectedLogs | Where-Object { Test-Path -LiteralPath $_ }).Count
                    $elapsed = [Math]::Round(([DateTime]::UtcNow - $process.StartTime.ToUniversalTime()).TotalSeconds)
                    Write-Host "  PID $($process.Id): ${elapsed}s, $ready/4 resultatfiler klare"
                    $nextProgress = [DateTime]::UtcNow.AddSeconds(10)
                }
            }
        }
        finally {
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
                $process.WaitForExit(5000) | Out-Null
            }
        }

        foreach ($sourceName in @("perf_log.csv", "land_log.csv", "parity_log.csv", "enemy_plane_log.csv")) {
            $source = Join-Path $OutDir $sourceName
            if (-not (Test-Path -LiteralPath $source)) {
                throw "Skill $Skill mangler forventet resultat: $sourceName"
            }
            $stem = [System.IO.Path]::GetFileNameWithoutExtension($sourceName)
            $tagSuffix = if ($ResultTag) { "_${ResultTag}" } else { "" }
            if ($Machine -ne 'A500') { $tagSuffix = "_${Machine}$tagSuffix" }
            Copy-Item -LiteralPath $source -Destination (Join-Path $ResultDir "${stem}_skill_${Skill}_speed_${CruiseSpeed}_wing_${WingmanControl}_seed_${SessionSeed}_enemy_${EnemyPlaneRate}x${tagSuffix}.csv") -Force
        }
        if ($ExtraCcFlags -match '(?:^|\s)-DHAR_HEADLESS_DAMAGE_EXERCISE=1(?:\s|$)') {
            $damageLines = @(Get-Content -LiteralPath (Join-Path $OutDir 'perf_log.csv') |
                Where-Object { $_.StartsWith('#damage_exercise,') })
            if ($damageLines.Count -ne 1) { throw 'Skadetesten mangler ett entydig resultat.' }
            $damageFields = $damageLines[0].Split(',')
            if ($damageFields.Count -ne 9) { throw "Ugyldig skaderesultat: $($damageLines[0])" }
            [uint32[]]$damage = $damageFields[1..8]
            $classicDamage = $ExtraCcFlags -match '(?:^|\s)-DHAR_HEADLESS_GAME_MODE=0(?:\s|$)'
            if ($damage[0] -eq 0 -or $damage[1] -eq 0 -or $damage[2] -eq 0 -or
                $damage[3] -ne 0 -or $damage[4] -lt $damage[1] -or
                $damage[5] -lt $damage[4] -or $damage[6] -le $damage[5] -or
                (-not $classicDamage -and $damage[7] -eq 0)) {
                throw "Skadetesten fullforte ikke skade, havari og avslutning: $($damageLines[0])"
            }
            Write-Host "Skadetest bestatt: $($damageLines[0])"
        }
        }
    }
}
finally {
    Write-Host "Gjenoppretter vanlig F5/release-bygg uten testflagg..."
    & $Make -C $AmigaDir clean
    & (Join-Path $Root "amiga-build.ps1") -Target build -Program "out/harrier_amiga"
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Vanlig Amiga-bygg feilet med exitkode $LASTEXITCODE."
    }
}

Write-Host "Ferdig. Resultater ligger i $ResultDir som *_skill_N_speed_N_wing_N_seed_N_enemy_Nx.csv."
