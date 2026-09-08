#Requires -Version 5.1
[CmdletBinding()]
param(
    [switch]$Check
)

$ErrorActionPreference = "Stop"
$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$Editor = Join-Path $Root "tools\amiga-graphics-editor.py"
$Python = Get-Command python.exe -ErrorAction SilentlyContinue
$PythonArguments = @()
if (-not $Python) {
    $Python = Get-Command py.exe -ErrorAction SilentlyContinue
    $PythonArguments = @("-3")
}
if (-not $Python) {
    throw "Fant ikke Python 3. Installer Python med Tkinter og Pillow for a bruke grafikkeditoren."
}

& $Python.Source @PythonArguments -c "import tkinter; import PIL"
if ($LASTEXITCODE -ne 0) {
    throw "Grafikkeditoren krever en Python 3-installasjon med Tkinter og Pillow."
}

if ($Check) {
    $PythonArguments += @($Editor, "--check")
} else {
    $PythonArguments += $Editor
}
& $Python.Source @PythonArguments
exit $LASTEXITCODE
