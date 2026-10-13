#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Command,
    [ValidatePattern('^WinUAE(?:_[0-9]+)?$')]
    [string]$PipeName = 'WinUAE',
    # Refuse commands if the pipe belongs to another (possibly user-owned) run.
    [uint32]$ExpectedProcessId = 0,
    [ValidateRange(100, 10000)]
    [int]$TimeoutMilliseconds = 3000
)

# WinUAE's native IPC API, not keyboard/UI automation. Protocol reference:
# https://github.com/tonioni/WinUAE/blob/master/uaeipc.cpp
# Use a dedicated capture run: enabling recording affects host I/O timing.
$ErrorActionPreference = 'Stop'
if ($Command.Contains([char]0)) { throw 'NUL is not allowed inside a command.' }
$pipe = [IO.Pipes.NamedPipeClientStream]::new(
    '.', $PipeName, [IO.Pipes.PipeDirection]::InOut,
    [IO.Pipes.PipeOptions]::Asynchronous)
try {
    $pipe.Connect($TimeoutMilliseconds)
    if ($ExpectedProcessId) {
        if (-not ('HarrierPipeOwner' -as [type])) {
            Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class HarrierPipeOwner {
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool GetNamedPipeServerProcessId(IntPtr pipe, out uint processId);
}
'@
        }
        [uint32]$serverProcessId = 0
        if (-not [HarrierPipeOwner]::GetNamedPipeServerProcessId(
            $pipe.SafePipeHandle.DangerousGetHandle(), [ref]$serverProcessId)) {
            throw 'Cannot verify WinUAE pipe owner; no command was sent.'
        }
        if ($serverProcessId -ne $ExpectedProcessId) {
            throw "WinUAE pipe belongs to PID $serverProcessId, expected $ExpectedProcessId; no command was sent."
        }
    }
    $encoding = [Text.UTF8Encoding]::new($true)
    [byte[]]$request = $encoding.GetPreamble() + $encoding.GetBytes($Command + [char]0)
    $write = $pipe.WriteAsync($request, 0, $request.Length)
    if (-not $write.Wait($TimeoutMilliseconds)) { throw 'WinUAE IPC write timed out.' }
    [byte[]]$buffer = New-Object byte[] 16384
    $read = $pipe.ReadAsync($buffer, 0, $buffer.Length)
    if (-not $read.Wait($TimeoutMilliseconds)) { throw 'WinUAE IPC reply timed out.' }
    $count = $read.Result
    if ($count -eq 0) { throw 'WinUAE closed IPC without a reply.' }
    $encoding.GetString($buffer, 0, $count).TrimEnd([char]0)
}
finally {
    $pipe.Dispose()
}
