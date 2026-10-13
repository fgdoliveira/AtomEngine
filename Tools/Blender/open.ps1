# Opens Blender with the AtomEngine panel (v0.0.17, M115): the 3D view's
# sidebar (N), tab AtomEngine - add spawn and entity markers, see what the
# level file says about the selected one, check them all.
#
#   pwsh Tools/Blender/open.ps1                          an empty scene
#   pwsh Tools/Blender/open.ps1 Apps/Samples/HelloAtom/Source/hello_room.blend
#
# The panel runs from the repository (it uses the export's marker code), so
# nothing is installed into Blender.
param([string]$Scene = "")
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
. (Join-Path $root "Tools/Dev/common.ps1")
$blender = Find-Blender
if (-not $blender) {
    Write-Host "Blender not found: install it, put it on PATH or set ATOM_BLENDER to blender.exe" -ForegroundColor Red
    exit 2
}
$arguments = @()
if ($Scene) { $arguments += (Resolve-Path $Scene).Path }
$arguments += @("--python", (Join-Path $root "Tools/Blender/atom_markers_panel.py"))
Start-Process $blender -ArgumentList ($arguments | ForEach-Object { "`"$_`"" })
