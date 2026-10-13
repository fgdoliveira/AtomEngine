# Removes personal data from a .blend before it is committed (v0.0.17,
# M116): Blender saves its UI with a scene, including the file browser's
# last folders (C:\Users\<you>\...). This keeps the scene data only, then
# checks that no absolute path is left. Run it after saving in Blender.
#
#   pwsh Tools/Blender/clean_scene.ps1 Apps/Samples/HelloAtom/Source/hello_room.blend
#   pwsh Tools/Blender/clean_scene.ps1 <scene.blend> -Check     check only
param([Parameter(Mandatory)] [string]$Scene, [switch]$Check)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
. (Join-Path $root "Tools/Dev/common.ps1")
$blender = Find-Blender
if (-not $blender) { Write-Host "Blender not found (ATOM_BLENDER, PATH or Program Files)" -ForegroundColor Red; exit 2 }
$arguments = @("-b", "--factory-startup", (Resolve-Path $Scene).Path, "--python", (Join-Path $root "Tools/Blender/clean_scene.py"))
if ($Check) { $arguments += @("--", "--check") }
& $blender @arguments 2>&1 | ForEach-Object { "$_" } | Where-Object { $_ -match '^(cleaned|clean|PERSONAL|not clean)|Error' }
exit $LASTEXITCODE
