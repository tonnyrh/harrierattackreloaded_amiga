#Requires -Version 5.1
# Starts the currently built game in WinUAE (does not rebuild - run
# .\amiga-build.ps1 first if you changed anything). Stops any previous
# instance first, since only one can hold the emulator window/ports.

$ErrorActionPreference = "Stop"

$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$UaeConfig = Join-Path $Root "amiga\harrier_headless.uae"

if (-not (Test-Path -LiteralPath $UaeConfig)) {
    throw "Fant ikke $UaeConfig"
}

$ExtensionRoot = Join-Path $env:USERPROFILE ".vscode\extensions"
$extension = Get-ChildItem -Directory $ExtensionRoot -Filter "bartmanabyss.amiga-debug-*" -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    Select-Object -First 1

if (-not $extension) {
    throw "Fant ikke VS Code-extension BartmanAbyss.amiga-debug under $ExtensionRoot. Kjor .\setup-dev-amiga.ps1 forst."
}

$exe = Join-Path $extension.FullName "bin\win32\winuae-gdb.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Fant ikke winuae-gdb.exe: $exe"
}

Get-Process winuae-gdb -ErrorAction SilentlyContinue | Stop-Process -Force

Start-Process -FilePath $exe -ArgumentList @("-f", $UaeConfig) -PassThru |
    Select-Object Id, ProcessName
