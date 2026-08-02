#Requires -Version 5.1
[CmdletBinding()]
param(
    [switch]$SkipExtensionInstall,
    [switch]$SkipTemplateDownload,
    [switch]$RefreshTemplate,
    [switch]$OpenVSCode
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$AmigaDir = Join-Path $Root "amiga"
$VsCodeDir = Join-Path $Root ".vscode"
$KickDir = Join-Path $Root ".tools\Amiga\Kick"
$PreferredKick = Join-Path $KickDir "Kickstart1.3.rom"

function Write-Step {
    param([string]$Message)
    Write-Host $Message -ForegroundColor Cyan
}

function Write-Ok {
    param([string]$Message)
    Write-Host $Message -ForegroundColor Green
}

function Write-Warn {
    param([string]$Message)
    Write-Host $Message -ForegroundColor Yellow
}

function Ensure-Directory {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Ensure-BartmanToolchainPath {
    $extensionRoot = Join-Path $env:USERPROFILE ".vscode\extensions"
    $extension = Get-ChildItem -LiteralPath $extensionRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object Name -Like "bartmanabyss.amiga-debug-*" |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1

    if (-not $extension) {
        throw "Fant ikke BartmanAbyss.amiga-debug under $extensionRoot. Installer extensionen og kjor oppsettet pa nytt."
    }

    $bin = Join-Path $extension.FullName "bin\win32"
    $compilerBin = Join-Path $bin "opt\bin"
    $make = Join-Path $bin "gnumake.exe"
    $compiler = Join-Path $compilerBin "m68k-amiga-elf-gcc.exe"
    if (-not (Test-Path -LiteralPath $make) -or
        -not (Test-Path -LiteralPath $compiler)) {
        throw "Bartman-extensionen finnes, men verktoykjeden er ikke komplett under $bin. Start VS Code en gang og kjor oppsettet pa nytt."
    }

    $toolPaths = @($compilerBin, $bin)
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $entries = @($userPath -split ";" | Where-Object { $_ -and $_.Trim() })

    # VS Code keeps the version in the extension directory name. Remove paths
    # belonging to an older Bartman version before adding the current one.
    $entries = @($entries | Where-Object {
        $_ -notmatch "(?i)[\\/]\.vscode[\\/]extensions[\\/]bartmanabyss\.amiga-debug-[^\\/;]+[\\/]bin[\\/]win32(?:[\\/]opt[\\/]bin)?[\\/]?$"
    })
    foreach ($toolPath in $toolPaths) {
        if (-not ($entries | Where-Object {
            [string]::Equals($_.TrimEnd([char[]]"\/"), $toolPath.TrimEnd([char[]]"\/"),
                [System.StringComparison]::OrdinalIgnoreCase)
        })) {
            $entries += $toolPath
        }
        if (-not (($env:PATH -split ";") | Where-Object {
            [string]::Equals($_.TrimEnd([char[]]"\/"), $toolPath.TrimEnd([char[]]"\/"),
                [System.StringComparison]::OrdinalIgnoreCase)
        })) {
            $env:PATH = "$toolPath;$env:PATH"
        }
    }

    $newUserPath = $entries -join ";"
    if ($newUserPath -ne $userPath) {
        [Environment]::SetEnvironmentVariable("Path", $newUserPath, "User")
        Write-Ok "Bartman-verktoykjeden er lagt permanent til i bruker-PATH."
        Write-Host "Apne en ny terminal (eller start VS Code pa nytt) for at andre prosesser skal arve PATH." -ForegroundColor Gray
    } else {
        Write-Ok "Bartman-verktoykjeden finnes allerede i bruker-PATH."
    }
    Write-Host "  $compilerBin" -ForegroundColor Gray
    Write-Host "  $bin" -ForegroundColor Gray
}

function Resolve-Kickstart13 {
    if (-not (Test-Path -LiteralPath $KickDir)) {
        throw "Fant ikke Kickstart-mappen: $KickDir"
    }

    $candidates = @(
        "Kickstart1.3.rom",
        "amiga-os-130.rom",
        "kick13.rom",
        "kick.rom"
    )

    foreach ($name in $candidates) {
        $path = Join-Path $KickDir $name
        if (Test-Path -LiteralPath $path) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }

    throw "Fant ingen Kickstart 1.3-kandidat i $KickDir. For A500-debugging forventes f.eks. Kickstart1.3.rom."
}

function Ensure-VSCodeExtension {
    param([string]$ExtensionId)

    $code = Get-Command code -ErrorAction SilentlyContinue
    if (-not $code) {
        Write-Warn "Fant ikke VS Code-kommandoen 'code'. Kjor .\setup-dev-amiga-admin.ps1 hvis VS Code mangler."
        return
    }

    $installed = & code --list-extensions 2>$null
    if ($installed -contains $ExtensionId) {
        Write-Ok "VS Code extension finnes allerede: $ExtensionId"
        return
    }

    Write-Step "Installerer VS Code extension: $ExtensionId"
    & code --install-extension $ExtensionId
    if ($LASTEXITCODE -ne 0) {
        throw "Kunne ikke installere VS Code extension: $ExtensionId"
    }
}

function Remove-ConflictingAmigaDebugExtensions {
    $code = Get-Command code -ErrorAction SilentlyContinue
    if (-not $code) {
        return
    }

    # Both extensions register the same "amiga" debug type and commands. If
    # they are installed together, activation order decides which adapter F5
    # uses and Bartman may fail with "command ... already exists".
    $conflictingExtensions = @(
        "davidcanadasmazo.amiga-debug-arch"
    )
    $installed = @(& code --list-extensions 2>$null)
    foreach ($extensionId in $conflictingExtensions) {
        if ($installed -contains $extensionId) {
            Write-Step "Fjerner konkurrerende Amiga-debugger: $extensionId"
            & code --uninstall-extension $extensionId
            if ($LASTEXITCODE -ne 0) {
                throw "Kunne ikke fjerne konkurrerende VS Code extension: $extensionId"
            }
        }
    }
}

function Update-M68kIncludePath {
    $extensionRoot = Join-Path $env:USERPROFILE ".vscode\extensions"
    $extension = Get-ChildItem -LiteralPath $extensionRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object Name -Like "bartmanabyss.amiga-debug-*" |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if (-not $extension) {
        return
    }

    $sysInclude = Join-Path $extension.FullName "bin\win32\opt\m68k-amiga-elf\sys-include"
    if (-not (Test-Path -LiteralPath $sysInclude)) {
        return
    }

    $settingsPath = Join-Path $VsCodeDir "settings.json"
    if (-not (Test-Path -LiteralPath $settingsPath)) {
        return
    }

    $settings = Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json
    $settings | Add-Member -NotePropertyName "m68k.includePaths" -NotePropertyValue @($sysInclude) -Force
    $settings | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $settingsPath -Encoding UTF8
    Write-Ok "m68k include path peker pa Bartman-verktoykjeden."
}

function Write-TextFile {
    param(
        [string]$Path,
        [string]$Content
    )

    Ensure-Directory (Split-Path -Parent $Path)
    Set-Content -LiteralPath $Path -Value $Content -Encoding UTF8
}

function Install-BartmanTemplate {
    if ($SkipTemplateDownload) {
        Write-Warn "Hopper over nedlasting av Amiga-template."
        return
    }

    $makefile = Join-Path $AmigaDir "Makefile"
    if ((Test-Path -LiteralPath $makefile) -and -not $RefreshTemplate) {
        Write-Ok "Amiga-template finnes allerede i $AmigaDir"
        return
    }

    Write-Step "Laster ned Bartman/Abyss Amiga-template til $AmigaDir"
    Ensure-Directory $AmigaDir
    $headers = @{ "User-Agent" = "harrierattackreloaded-amiga-setup" }

    function Copy-GitHubDirectory {
        param(
            [string]$ApiUrl,
            [string]$Destination
        )

        Ensure-Directory $Destination
        $items = Invoke-RestMethod -Headers $headers -Uri $ApiUrl
        foreach ($item in $items) {
            if ($item.name -in @(".vscode", "obj", "out")) {
                continue
            }

            $target = Join-Path $Destination $item.name
            if ($item.type -eq "dir") {
                Copy-GitHubDirectory -ApiUrl $item.url -Destination $target
                continue
            }

            if ($item.type -eq "file") {
                if ((Test-Path -LiteralPath $target) -and -not $RefreshTemplate) {
                    continue
                }
                Invoke-WebRequest -Headers $headers -Uri $item.download_url -OutFile $target
            }
        }
    }

    Copy-GitHubDirectory -ApiUrl "https://api.github.com/repos/BartmanAbyss/vscode-amiga-debug/contents/template?ref=master" -Destination $AmigaDir
    Ensure-Directory (Join-Path $AmigaDir "obj")
    Ensure-Directory (Join-Path $AmigaDir "out")
    Write-Ok "Amiga-template er klar."
}

Write-Step "Setter opp Amiga 500 utviklingsmiljo"

$kick = Resolve-Kickstart13
$kickInfo = Get-Item -LiteralPath $kick
if ($kickInfo.Length -notin @(262144, 524288)) {
    Write-Warn "Kickstart-filen finnes, men storrelsen er uvanlig for KS1.3: $($kickInfo.Length) bytes"
}
Write-Ok "Kickstart funnet: $kick"

Ensure-Directory $VsCodeDir

Write-TextFile -Path (Join-Path $VsCodeDir "extensions.json") -Content @'
{
  "recommendations": [
    "BartmanAbyss.amiga-debug",
    "ms-vscode.cpptools",
    "gigabates.m68k-lsp"
  ]
}
'@

Write-TextFile -Path (Join-Path $VsCodeDir "c_cpp_properties.json") -Content @'
{
  "configurations": [
    {
      "name": "Amiga",
      "configurationProvider": "BartmanAbyss.amiga-debug"
    }
  ],
  "version": 4
}
'@

Write-TextFile -Path (Join-Path $VsCodeDir "amiga.json") -Content @'
{
  "includePath": [
    "${workspaceFolder}/amiga",
    "${workspaceFolder}/amiga/support"
  ],
  "defines": [
    "__INTELLISENSE__"
  ]
}
'@

Write-TextFile -Path (Join-Path $VsCodeDir "settings.json") -Content @'
{
  "amiga.program": "amiga/out/harrier_amiga",
  "amiga.rom-paths.A500": "${workspaceFolder}/.tools/Amiga/Kick/Kickstart1.3.rom",
  "files.associations": {
    "*.asm": "asm-m68k",
    "*.s": "asm-m68k",
    "*.i": "c"
  }
}
'@

Write-TextFile -Path (Join-Path $VsCodeDir "tasks.json") -Content @'
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "amiga: compile",
      "type": "process",
      "command": "${env:WINDIR}\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
      "options": {
        "cwd": "${workspaceFolder}"
      },
      "args": [
        "-NoLogo",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        "${workspaceFolder}\\amiga-build.ps1",
        "-Target",
        "build",
        "-Program",
        "out/harrier_amiga"
      ],
      "problemMatcher": [
        {
          "base": "$gcc",
          "fileLocation": "absolute"
        }
      ],
      "group": {
        "kind": "build",
        "isDefault": true
      },
      "presentation": {
        "echo": true,
        "reveal": "always",
        "focus": false,
        "panel": "shared",
        "showReuseMessage": false,
        "clear": true
      }
    },
    {
      "label": "amiga: clean",
      "type": "process",
      "command": "${env:WINDIR}\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
      "options": {
        "cwd": "${workspaceFolder}"
      },
      "args": [
        "-NoLogo",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        "${workspaceFolder}\\amiga-build.ps1",
        "-Target",
        "clean"
      ],
      "problemMatcher": []
    }
  ]
}
'@

Write-TextFile -Path (Join-Path $VsCodeDir "launch.json") -Content @'
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Amiga 500 debug (KS1.3, 1MB)",
      "type": "amiga",
      "request": "launch",
      "preLaunchTask": "amiga: compile",
      "config": "A500",
      "chipmem": "512k",
      "slowmem": "512k",
      "fastmem": "0",
      "program": "${workspaceFolder}/${config:amiga.program}",
      "kickstart": "${workspaceFolder}/.tools/Amiga/Kick/Kickstart1.3.rom",
      "internalConsoleOptions": "openOnSessionStart"
    }
  ]
}
'@

Install-BartmanTemplate

if (-not $SkipExtensionInstall) {
    Remove-ConflictingAmigaDebugExtensions
    Ensure-VSCodeExtension "BartmanAbyss.amiga-debug"
    Ensure-VSCodeExtension "ms-vscode.cpptools"
    Ensure-VSCodeExtension "gigabates.m68k-lsp"
}

Ensure-BartmanToolchainPath
Update-M68kIncludePath

Write-Ok "Ferdig. Apne repoet i VS Code, velg 'Amiga 500 debug (KS1.3, 1MB)' og trykk F5."
Write-Host "Forstegangskjoring kan bruke litt tid mens Bartman/Abyss-utvidelsen pakker ut sine egne verktøy." -ForegroundColor Gray

if ($OpenVSCode) {
    $code = Get-Command code -ErrorAction SilentlyContinue
    if ($code) {
        & code $Root
    }
}
