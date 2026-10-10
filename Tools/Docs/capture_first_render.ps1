# Documentation captures of the first-render scene, then GIFs of its
# frame sequences. Images: out/img/first_render/, GIFs: .../gifs/.
#
#   pwsh Tools/Docs/capture_first_render.ps1 [-Config Release]
#
# Needs a built game (cmake --build build --config <Config>) and Blender
# 5.2 for its bundled Python (numpy), used by make_gif.py.
param(
    [string]$Config = "Release",
    [string]$Blender = "C:\Program Files\Blender Foundation\Blender 5.2"
)
$ErrorActionPreference = "Stop"
$repo = (Resolve-Path "$PSScriptRoot\..\..").Path
$game = "$repo\build\bin\$Config\Showcase.exe"
$out = "$repo\out\img\first_render"

if (Test-Path $out) { Remove-Item -Recurse -Force $out }

$env:ATOM_ASSET_ROOT = $repo                      # read the source assets and write out/ in the repo
$env:ATOM_START_LEVEL = "first_render"
$env:ATOM_TEST_SCRIPT = "$PSScriptRoot\first_render.atomtest"
$process = Start-Process $game -WorkingDirectory (Split-Path $game) -PassThru -Wait -NoNewWindow
if ($process.ExitCode -ne 0) { throw "capture script failed (exit $($process.ExitCode))" }

$python = Get-ChildItem "$Blender\*\python\bin\python.exe" | Select-Object -First 1
& $python.FullName "$PSScriptRoot\make_gif.py" "$out\seq" "$out\gifs" 30
Write-Host "Captures in $out"
