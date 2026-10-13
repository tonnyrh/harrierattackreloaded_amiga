#Requires -Version 5.1
<#
Read-only host presentation measurement. Run from an elevated PowerShell or as
a member of Performance Log Users. Does not send IPC/input or change WinUAE.
Use a dedicated muted run. --qpc_time_ms uses the monotonic host clock, not
the emulated Amiga clock or the game's FPS counter.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][int]$EmulatorProcessId,
    [Parameter(Mandatory=$true)][string]$OutputCsv,
    [ValidateRange(2,120)][int]$Seconds=20,
    [string]$PresentMon=(Join-Path $PSScriptRoot '../.tools/PresentMon/PresentMon-2.6.0-x64.exe')
)
$ErrorActionPreference='Stop'
$emulator=Get-Process -Id $EmulatorProcessId
if ($emulator.ProcessName -notin @('winuae','winuae64','winuae-gdb')) {
    throw 'The selected process is not WinUAE.'
}
if (!(Test-Path -LiteralPath $PresentMon)) { throw 'PresentMon executable missing; see SCROLL_MEASUREMENT.md.' }
if (Test-Path -LiteralPath $OutputCsv) { throw 'Choose a new output filename; existing evidence will not be overwritten.' }
$outputPath=[IO.Path]::GetFullPath($OutputCsv)
New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($outputPath)) | Out-Null
$metadata=@{
    process_id=$EmulatorProcessId
    executable=$emulator.Path
    process_start_utc=$emulator.StartTime.ToUniversalTime().ToString('o')
    capture_start_utc=[DateTime]::UtcNow.ToString('o')
    qpc_ticks=[Diagnostics.Stopwatch]::GetTimestamp()
    qpc_frequency=[Diagnostics.Stopwatch]::Frequency
    requested_seconds=$Seconds
    note='Host presentation events; does not identify duplicated Amiga image contents.'
}
$session='HarrierPresent-'+[Guid]::NewGuid().ToString('N')
& $PresentMon --process_id $EmulatorProcessId --output_file $outputPath --qpc_time_ms --timed $Seconds --terminate_after_timed --no_console_stats --no_track_input --no_track_gpu --session_name $session
$captureExit=$LASTEXITCODE
$metadata['exit_code']=$captureExit
$metadata | ConvertTo-Json | Set-Content -LiteralPath ($outputPath+'.metadata.json') -Encoding UTF8
if ($captureExit -ne 0) { throw "PresentMon failed ($captureExit). ETW capture requires administrator or Performance Log Users permission." }
if (!(Test-Path -LiteralPath $outputPath)) { throw 'No CSV produced; measurement is not valid.' }
Write-Output "Saved host-clock trace: $outputPath"
