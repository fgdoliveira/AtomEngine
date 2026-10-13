# Exports content from its Blender source, then checks the levels that use
# it (v0.0.17, M113). The one command to run after editing a builder in
# Tools/Blender/ - by hand or by an agent.
#
#   pwsh Tools/Blender/export.ps1 -List                      the units
#   pwsh Tools/Blender/export.ps1 -Only shrine               one level's models, bakes, markers
#   pwsh Tools/Blender/export.ps1 -Only shrine -What markers only its markers (seconds)
#   pwsh Tools/Blender/export.ps1                            everything
#   options: -What all|geometry|markers, -NoCache (re-bake every lightmap),
#            -Gpu (preview lightmaps: never commit them), -NoCheck
#
# A unit is what one builder makes (build_assets.py --list): kit, sky,
# street, city, night, and each level folder. Unchanged outputs are not
# rewritten, so `git status` shows exactly what the edit changed. Then
# AtomLevelCheck (built by CMake) checks every app's levels; its errors fail
# the export. Exit code: 0 ok, 1 export or check failed, 2 tools missing.
param(
    [string[]]$Only = @(),
    [ValidateSet("all", "geometry", "markers")] [string]$What = "all",
    [switch]$List,
    [switch]$NoCache,
    [switch]$Gpu,
    [switch]$NoCheck,
    [string]$BuildDir = "build"
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
. (Join-Path $root "Tools/Dev/common.ps1")

$blender = Find-Blender
if (-not $blender) {
    Write-Host "Blender not found: install it, put it on PATH or set ATOM_BLENDER to blender.exe" -ForegroundColor Red
    exit 2
}

# "-Only a,b" arrives as one string through `pwsh -File`: split it.
$units = @($Only | ForEach-Object { $_ -split "," } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$arguments = @("-b", "--factory-startup", "-P", (Join-Path $root "Tools/Blender/build_assets.py"), "--")
if ($List) { $arguments += "--list" }
if ($units.Count) { $arguments += @("--only", ($units -join ",")) }
$arguments += @("--what", $What)
if ($NoCache) { $arguments += "--no-cache" }
if ($Gpu) { $arguments += "--gpu" }

$started = Get-Date
# Blender's own chatter (glTF exporter, Cycles) is left out; the build's
# lines and every error stay.
& $blender @arguments 2>&1 | ForEach-Object { "$_" } |
    Where-Object { $_ -notmatch '^\d\d:\d\d:\d\d \| INFO|^INFO |^Blender |^$|^Fra:|Blender quit' }
$exit = $LASTEXITCODE
if ($List) { exit $exit }
if ($exit -ne 0) {
    Write-Host "FAILED: the export (exit $exit)" -ForegroundColor Red
    exit 1
}
Write-Host ("export: {0:N0} s" -f ((Get-Date) - $started).TotalSeconds)
if ($Gpu) { Write-Host "GPU-baked lightmaps: preview only - re-export without -Gpu before committing" -ForegroundColor Yellow }

if ($NoCheck) { exit 0 }
$check = @("Debug", "Release") | ForEach-Object { Join-Path $root "$BuildDir/bin/$_/AtomLevelCheck.exe" } |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $check) {
    Write-Host "AtomLevelCheck not built (cmake --build $BuildDir --target AtomLevelCheck): levels not checked" -ForegroundColor Yellow
    exit 0
}
& $check --all --errors-only
if ($LASTEXITCODE -ne 0) {
    Write-Host "FAILED: the levels' check (run $check --all for the warnings too)" -ForegroundColor Red
    exit 1
}
exit 0
