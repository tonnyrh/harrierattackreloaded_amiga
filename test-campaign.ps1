#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateRange(15, 1200)]
    [int]$TimeoutSeconds = 900,
    [string]$ExtraCcFlags = "-DHAR_HEADLESS_CAMPAIGN_TEST=1"
)

$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
$AmigaDir = Join-Path $Root "amiga"
$Stage=Join-Path $Root '.tmp/campaign-contract-run'
New-Item -ItemType Directory -Path $Stage -Force | Out-Null
$Result = Join-Path $Stage 'classic_contract.txt'
$Config = Join-Path $Stage 'test.uae'
Get-Content (Join-Path $Root 'amiga/harrier_headless.uae') | ForEach-Object {
 if ($_ -match '^filesystem2=') { "filesystem2=rw,dh1:dh1:$Stage,-128" } else { $_ }
} | Set-Content -LiteralPath $Config -Encoding ASCII
Add-Content -LiteralPath $Config -Value 'sound_volume=100' -Encoding ASCII
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
    Remove-Item -LiteralPath (Join-Path $AmigaDir "obj/main.o") -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $Result) {
        Remove-Item -LiteralPath $Result -Force
    }

    $flags = "-DHAR_HEADLESS_CLASSIC_CONTRACT_TEST=1 -DHAR_HIGHSCORE_DISK_IO=0 $ExtraCcFlags"
    & $Make -C $AmigaDir -j4 "program=../.tmp/campaign-contract-run/harrier_amiga" "EXTRA_CCFLAGS=$flags"
    if ($LASTEXITCODE -ne 0) { throw "Contract-bygg feilet: $LASTEXITCODE" }

    $process = Start-Process -FilePath $WinUae -ArgumentList @("-f", $Config) -WindowStyle Hidden -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    # DOS creates the file before its first Write completes. Wait for data,
    # otherwise Get-Content -Raw can return null and abort a valid short test.
    while (-not (Test-Path -LiteralPath $Result) -or
        (Get-Item -LiteralPath $Result).Length -eq 0) {
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
    Write-Host "Classic contract result: $contractResult"
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
    $fixture = Join-Path $AmigaDir 'out/carrier-status-preview.bpl'
    if(Test-Path -LiteralPath $fixture) { Copy-Item -LiteralPath $fixture -Destination (Join-Path $Root '.tmp/carrier-status-preview.bpl') }
    Get-ChildItem -LiteralPath (Join-Path $AmigaDir 'out') -Filter 'carrier_hud_*.bpl' | Copy-Item -Destination (Join-Path $Root '.tmp')
}
