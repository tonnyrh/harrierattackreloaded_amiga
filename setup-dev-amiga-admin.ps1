#Requires -RunAsAdministrator
#Requires -Version 5.1
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

function Find-Tool {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [string[]]$KnownPaths = @()
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    foreach ($path in $KnownPaths) {
        if (Test-Path -LiteralPath $path) {
            return $path
        }
    }

    return $null
}

function Ensure-WingetPackage {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ToolName,
        [Parameter(Mandatory = $true)]
        [string]$PackageId,
        [string[]]$KnownPaths = @()
    )

    $existing = Find-Tool -Name $ToolName -KnownPaths $KnownPaths
    if ($existing) {
        Write-Host "$ToolName finnes allerede: $existing" -ForegroundColor Green
        return
    }

    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw "winget mangler. Installer App Installer fra Microsoft Store, eller installer $ToolName manuelt."
    }

    Write-Host "Installerer $PackageId ..." -ForegroundColor Cyan
    winget install --id $PackageId -e --silent --accept-source-agreements --accept-package-agreements
    $exitCode = $LASTEXITCODE

    $installed = Find-Tool -Name $ToolName -KnownPaths $KnownPaths
    if ($exitCode -ne 0 -and -not $installed) {
        throw "winget install feilet for $PackageId med exit code $exitCode."
    }

    if ($installed) {
        Write-Host "$ToolName er installert: $installed" -ForegroundColor Green
        return
    }

    Write-Warning "$PackageId ble installert, men $ToolName finnes ikke i denne PowerShell-sesjonen enna. Apne en ny terminal og prov igjen."
}

$vsCodePaths = @(
    (Join-Path $env:LOCALAPPDATA "Programs\Microsoft VS Code\bin\code.cmd"),
    "C:\Program Files\Microsoft VS Code\bin\code.cmd"
)

$gitPaths = @(
    "C:\Program Files\Git\cmd\git.exe",
    "C:\Program Files (x86)\Git\cmd\git.exe"
)

Ensure-WingetPackage -ToolName "code" -PackageId "Microsoft.VisualStudioCode" -KnownPaths $vsCodePaths
Ensure-WingetPackage -ToolName "git" -PackageId "Git.Git" -KnownPaths $gitPaths

Write-Host "Admin-avhengigheter er OK. Kjor deretter .\setup-dev-amiga.ps1 uten admin." -ForegroundColor Green
