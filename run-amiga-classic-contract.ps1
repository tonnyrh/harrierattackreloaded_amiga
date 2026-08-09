#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateRange(15, 120)]
    [int]$TimeoutSeconds = 45
)

$ErrorActionPreference = "Stop"
$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$AmigaDir = Join-Path $Root "amiga"
$Result = Join-Path $AmigaDir "out\classic_contract.txt"
$Config = Join-Path $AmigaDir "harrier_headless.uae"
$Extension = Get-ChildItem -Directory (Join-Path $env:USERPROFILE ".vscode\extensions") `
    -Filter "bartmanabyss.amiga-debug-*" -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | Select-Object -First 1

if (-not $Extension) {
    throw "Fant ikke Bartman/Abyss Amiga-utvidelsen."
}

$Bin = Join-Path $Extension.FullName "bin\win32"
$Make = Join-Path $Bin "gnumake.exe"
$WinUae = Join-Path $Bin "winuae-gdb.exe"
foreach ($required in @($Make, $WinUae, $Config)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Mangler nodvendig fil: $required"
    }
}

$env:PATH = "$Bin\opt\bin;$Bin;$env:PATH"
$process = $null

try {
    & $Make -C $AmigaDir clean
    if ($LASTEXITCODE -ne 0) { throw "Clean feilet: $LASTEXITCODE" }
    if (Test-Path -LiteralPath $Result) {
        Remove-Item -LiteralPath $Result -Force
    }

    $flags = "-DHAR_HEADLESS_CLASSIC_CONTRACT_TEST=1 -DHAR_HIGHSCORE_DISK_IO=0"
    & $Make -C $AmigaDir -j4 "program=out/harrier_amiga" "EXTRA_CCFLAGS=$flags"
    if ($LASTEXITCODE -ne 0) { throw "Contract-bygg feilet: $LASTEXITCODE" }

    $process = Start-Process -FilePath $WinUae -ArgumentList @("-f", $Config) -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not (Test-Path -LiteralPath $Result)) {
        if ([DateTime]::UtcNow -ge $deadline) {
            throw "Classic contract ga ikke resultat innen $TimeoutSeconds sekunder."
        }
        if ($process.HasExited) {
            throw "WinUAE avsluttet uten classic_contract.txt."
        }
        Start-Sleep -Milliseconds 250
        $process.Refresh()
    }

    $contractResult = (Get-Content -LiteralPath $Result -Raw).Trim()
    if (-not $contractResult.StartsWith("PASS", [System.StringComparison]::Ordinal)) {
        throw "Classic contract feilet: $contractResult"
    }
    Write-Host "Classic contract bestatt: $contractResult"
}
finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(5000) | Out-Null
    }
    Write-Host "Gjenoppretter vanlig F5/release-bygg..."
    & $Make -C $AmigaDir clean
    & (Join-Path $Root "amiga-build.ps1") -Target build -Program "out/harrier_amiga"
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Vanlig Amiga-bygg feilet med exitkode $LASTEXITCODE."
    }
}
