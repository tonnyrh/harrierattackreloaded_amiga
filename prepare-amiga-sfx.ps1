#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$SourceDirectory,
    [switch]$NoGenerateMissing,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
if (-not $SourceDirectory) {
    $SourceDirectory = Join-Path $Root "amiga\assets\sfx-sourcefiles"
}
$SourceDirectory = [System.IO.Path]::GetFullPath($SourceDirectory)
$RuntimeDirectory = Join-Path $Root "amiga\assets\sfx"
$PreviewDirectory = Join-Path $Root ".tmp\amiga-sfx-preview"
$Converter = Join-Path $Root "tools\wav_to_amiga_sfx.py"

$AudioGenSkillRoot = if ($env:AUDIOGEN_SKILL_ROOT) {
    $env:AUDIOGEN_SKILL_ROOT
} else {
    "C:\vscode\AudioGenSkill"
}
$AudioGen = Join-Path $AudioGenSkillRoot "skills\audiogen\scripts\generate_sfx.py"

# Keep amiga/assets/AUDIO.md in sync when this manifest or an event assignment
# changes. That document is the canonical human-readable usage overview.
$Sounds = @(
    @{
        Source = "rocket.wav"; Runtime = "fire.raw"; Preview = "fire.wav"
        Duration = 0.55; MaxMs = 700
        Prompt = "single short dry rocket launch, compact ignition snap and smooth exhaust burst, isolated game sound, no explosion, music or voice"
    },
    @{
        Source = "bomb.wav"; Runtime = "bomb.raw"; Preview = "bomb.wav"
        Duration = 0.55; MaxMs = 700
        Prompt = "single short aircraft bomb release, mechanical click and sleek descending air whistle, isolated game sound, no explosion, music or voice"
    },
    @{
        Source = "impact.wav"; Runtime = "impact.raw"; Preview = "impact.wav"
        Duration = 0.65; MaxMs = 700
        Prompt = "single compact explosion with a short debris thump, dry isolated arcade game impact, no music or voice"
    },
    @{
        Source = "player_hit.wav"; Runtime = "hit.raw"; Preview = "hit.wav"
        Duration = 0.45; MaxMs = 550
        Prompt = "single short metallic aircraft damage crack and warning zap, dry isolated arcade game sound, no music or voice"
    },
    @{
        Source = "eject.wav"; Runtime = "eject.raw"; Preview = "eject.wav"
        Duration = 0.65; MaxMs = 800
        Prompt = "single aircraft ejection seat launch, sharp cartridge blast followed by a brief mechanical air rush, dry isolated game sound, no music or voice"
    },
    @{
        Source = "pickpuppowerup.wav"; Runtime = "pickup_powerup.raw"; Preview = "pickup_powerup.wav"
        Duration = 0.55; MaxMs = 700
        Prompt = "single bright arcade powerup pickup confirmation, short clean rising electronic sparkle, isolated game sound, no music or voice"
    },
    @{
        Source = "flak_0.wav"; Runtime = "flak_gun_1.raw"; Preview = "flak_gun_1.wav"
        Duration = 0.5; MaxMs = 700
        Prompt = "single short anti-aircraft cannon shot, dry sharp mechanical blast, isolated game sound, no echo, music or voice"
    },
    @{
        Source = "flak_1.wav"; Runtime = "flak_gun_2.raw"; Preview = "flak_gun_2.wav"
        Duration = 0.5; MaxMs = 700
        Prompt = "single compact flak gun firing, hard muzzle crack and brief low thump, isolated game sound, no echo, music or voice"
    },
    @{
        Source = "ground_hit_0.wav"; Runtime = "ground_target_hit_1.raw"; Preview = "ground_target_hit_1.wav"
        Duration = 0.6; MaxMs = 700; BassBoost = 1.0
        Prompt = "single short metal ground target impact, sharp strike and compact debris crunch, isolated game sound, no music or voice"
    },
    @{
        Source = "ground_hit_1.wav"; Runtime = "ground_target_hit_2.raw"; Preview = "ground_target_hit_2.wav"
        Duration = 0.6; MaxMs = 700; BassBoost = 1.0
        Prompt = "single concrete ground target hit, dry crack and brief rubble thump, isolated game sound, no music or voice"
    },
    @{
        Source = "ground_hit_2.wav"; Runtime = "ground_target_hit_3.raw"; Preview = "ground_target_hit_3.wav"
        Duration = 0.6; MaxMs = 700; BassBoost = 1.0
        Prompt = "single armored vehicle impact, hard metallic clang and short debris burst, isolated game sound, no music or voice"
    },
    @{
        Source = "ground_hit_3.wav"; Runtime = "ground_target_hit_4.raw"; Preview = "ground_target_hit_4.wav"
        Duration = 0.6; MaxMs = 700; BassBoost = 1.0
        Prompt = "single building or ship impact, compact crunch and low dry thud, isolated game sound, no music or voice"
    },
    @{
        Source = "idle_0.wav"; Runtime = "carrier_idle_1.adpcm"; Preview = "carrier_idle_1.wav"; Adpcm = $true
        Duration = 4.0; MaxMs = 4000
        Prompt = "quiet aircraft carrier deck ambience, distant machinery and wind, isolated subtle game ambience, no music or voice"
    },
    @{
        Source = "idle_1.wav"; Runtime = "carrier_idle_2.adpcm"; Preview = "carrier_idle_2.wav"; Adpcm = $true
        Duration = 4.0; MaxMs = 4000
        Prompt = "quiet aircraft carrier deck ambience, distant metal movement and sea wind, isolated subtle game ambience, no music or voice"
    }
)

foreach ($directory in @($SourceDirectory, $RuntimeDirectory, $PreviewDirectory)) {
    if (-not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Path $directory | Out-Null
    }
}

if (-not (Test-Path -LiteralPath $Converter)) {
    throw "Fant ikke WAV-til-Paula-konvertereren: $Converter"
}

foreach ($sound in $Sounds) {
    $source = Join-Path $SourceDirectory $sound.Source
    $runtime = Join-Path $RuntimeDirectory $sound.Runtime
    $preview = Join-Path $PreviewDirectory $sound.Preview

    if (-not (Test-Path -LiteralPath $source)) {
        if ($NoGenerateMissing) {
            throw "Mangler lydmaster: $source"
        }
        if (-not (Test-Path -LiteralPath $AudioGen)) {
            throw "Mangler $source og fant ikke AudioGen-generatoren: $AudioGen"
        }

        Write-Host "AudioGen lager manglende lydmaster: $source"
        $outputBase = Join-Path $SourceDirectory ([System.IO.Path]::GetFileNameWithoutExtension($sound.Source))
        # generate_sfx.py refuses existing output paths by default. Deliberately
        # do not pass --overwrite here: a build may fill holes, never replace a
        # WAV selected or edited by the user.
        & python $AudioGen $sound.Prompt --duration $sound.Duration --output $outputBase
        if ($LASTEXITCODE -ne 0) {
            throw "AudioGen feilet for $source med exitkode $LASTEXITCODE"
        }
    }

    $needsConversion = $Force -or
        -not (Test-Path -LiteralPath $runtime) -or
        -not (Test-Path -LiteralPath $preview)
    if (-not $needsConversion) {
        $derivedTime = (Get-Item -LiteralPath $runtime).LastWriteTimeUtc
        $needsConversion =
            (Get-Item -LiteralPath $source).LastWriteTimeUtc -gt $derivedTime -or
            (Get-Item -LiteralPath $Converter).LastWriteTimeUtc -gt $derivedTime
    }

    if ($needsConversion) {
        Write-Host "Konverterer $($sound.Source) -> $($sound.Runtime)"
        $converterArguments = @(
            $Converter, $source, $runtime,
            "--rate", 11025,
            "--max-ms", $sound.MaxMs,
            "--preview-wav", $preview
        )
        if ($sound.ContainsKey("BassBoost")) {
            $converterArguments += @("--bass-boost", $sound.BassBoost)
        }
        if ($sound.ContainsKey("Adpcm") -and $sound.Adpcm) {
            $converterArguments += "--adpcm"
        }
        & python @converterArguments
        if ($LASTEXITCODE -ne 0) {
            throw "Paula-konvertering feilet for $source med exitkode $LASTEXITCODE"
        }
    }
}

Write-Host "Amiga-lydbanken er klar. WAV-mastere: $SourceDirectory"
Write-Host "Eksakt Paula-preview: $PreviewDirectory"
