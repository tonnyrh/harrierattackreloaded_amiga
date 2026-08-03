#Requires -Version 5.1
[CmdletBinding()]
param(
    [int[]]$Skills = @(1, 3, 5),
    [ValidateRange(1, 15)]
    [int]$CruiseSpeed = 15,
    [ValidateRange(0, 2)]
    [int]$WingmanControl = 1,
    [ValidateRange(1, 65535)]
    [int]$SessionSeed = 12040,
    [int[]]$EnemyPlaneRates = @(1),
    [switch]$EnemyPlaneExercise,
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

$env:PATH = "$env:PATH;$Bin\opt\bin;$Bin"
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
New-Item -ItemType Directory -Path $ResultDir -Force | Out-Null

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

        Write-Host "Bygger og maaler Amiga skill $Skill, enemy ${EnemyPlaneRate}x (cycle-exact A500 + 512K)..."
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

        $process = Start-Process -FilePath $WinUae -ArgumentList @("-f", $Config) -PassThru
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
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
            Copy-Item -LiteralPath $source -Destination (Join-Path $ResultDir "${stem}_skill_${Skill}_speed_${CruiseSpeed}_wing_${WingmanControl}_seed_${SessionSeed}_enemy_${EnemyPlaneRate}x${tagSuffix}.csv") -Force
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
