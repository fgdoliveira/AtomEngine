# Documentation captures of the character lab (v0.0.6), then GIFs of its
# frame sequences. Images: out/img/character_lab/, GIFs: .../gifs/.
#
#   pwsh Tools/Docs/capture_character_lab.ps1 [-Config Release]
#
# Needs a built game (cmake --build build --config <Config>) and Blender
# 5.2 for its bundled Python (numpy), used by make_gif.py.
param(
    [string]$Config = "Release",
    [string]$Blender = "C:\Program Files\Blender Foundation\Blender 5.2"
)
$ErrorActionPreference = "Stop"
$repo = (Resolve-Path "$PSScriptRoot\..\..").Path
$game = "$repo\build\bin\$Config\AtomGame.exe"
$out = "$repo\out\img\character_lab"

if (Test-Path $out) { Remove-Item -Recurse -Force $out }

$env:ATOM_ASSET_ROOT = $repo                      # read Assets/ and write out/ in the repo
$env:ATOM_START_LEVEL = "character_lab"
$env:ATOM_TEST_SCRIPT = "$PSScriptRoot\character_lab.atomtest"
$process = Start-Process $game -WorkingDirectory (Split-Path $game) -PassThru -Wait -NoNewWindow
if ($process.ExitCode -ne 0) { throw "capture script failed (exit $($process.ExitCode))" }

$python = Get-ChildItem "$Blender\*\python\bin\python.exe" | Select-Object -First 1
& $python.FullName "$PSScriptRoot\make_gif.py" "$out\seq" "$out\gifs" 30
Write-Host "Captures in $out"
