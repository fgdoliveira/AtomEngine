# The source SDK used as a stranger would (v0.0.20, M132): from the zip,
# outside the repository, a game in a folder of its own.
#
#   pwsh Tools/Dist/test-sdk.ps1 [-Zip Dist/AtomSDK-v<version>-src.zip] [-Configs Debug,Release]
#
#   1. extracts the zip into a fresh temporary folder (a space in its path);
#   2. builds that SDK's AtomProjectTool - nothing of the repository's;
#   3. creates MyGame in another folder with it (its CMakeUserPresets.json
#      names the extracted SDK);
#   4. makes it HelloAtom, the way docs/Getting-Started.md section 7 says: its
#      Main.cpp with the project's asset folder, its level, the shared
#      content it uses (CONTENT First Kit);
#   5. configures it through its preset, builds each configuration, and
#      runs it there with HelloAtom's smoke scenario (exit 0, GPU needed).
# Minutes: the engine compiles inside the game, once per configuration.
param(
    [string]$Zip = "",
    [string[]]$Configs = @("Debug", "Release")
)
$ErrorActionPreference = "Stop"
$started = Get-Date
$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
Set-Location $root
. "$PSScriptRoot/../Dev/common.ps1"
$cmake = Find-DevTool "cmake"
if (-not $cmake) { throw "cmake not found: run Tools/Dev/doctor.ps1" }
if (-not $Zip) {
    $Zip = Get-ChildItem Dist -Filter "AtomSDK-v*-src.zip" | Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
    if (-not $Zip) { throw "no SDK zip in Dist/: run Tools/Dist/sdk.ps1 first" }
}
$Configs = @($Configs | ForEach-Object { $_ -split "," } | Where-Object { $_ })

function Step([string]$what, [scriptblock]$action) {
    Write-Host "== $what"
    $out = & $action 2>&1
    if ($LASTEXITCODE -ne 0) {
        $out | Select-Object -Last 25 | ForEach-Object { Write-Host "   $_" }
        Write-Host "FAILED: $what (exit $LASTEXITCODE)" -ForegroundColor Red
        exit 1
    }
}

$work = Join-Path ([System.IO.Path]::GetTempPath()) "atom sdk test"
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
$extracted = Join-Path $work "sdk"
$games = Join-Path $work "my games"
New-Item -ItemType Directory -Force $extracted, $games | Out-Null

Step "extract $(Split-Path $Zip -Leaf)" { & $cmake -E chdir $extracted $cmake -E tar xf $Zip }
$sdk = (Get-ChildItem $extracted -Directory | Select-Object -First 1).FullName
$hello = Join-Path $sdk "Apps/Samples/HelloAtom"

# 2. The SDK's own tool (its engine root is the extracted folder).
$toolBuild = Join-Path $work "sdk build"
Step "configure the SDK (its tools)" { & $cmake -S $sdk -B $toolBuild -DATOM_BUILD_SHADERS=OFF -DATOM_EXAMPLE_APPS=OFF }
Step "build AtomProjectTool" { & $cmake --build $toolBuild --config Release --target AtomProjectTool }
$tool = Join-Path $toolBuild "bin/Release/AtomProjectTool.exe"

# 3. A game in its own folder.
Step "create MyGame" { & $tool create --in $games --name MyGame }
$game = Join-Path $games "MyGame"
$userPresets = Get-Content (Join-Path $game "CMakeUserPresets.json") -Raw
if (-not $userPresets.Contains($sdk.Replace("\", "/"))) { Write-Host "FAILED: CMakeUserPresets.json doesn't name the extracted SDK" -ForegroundColor Red; exit 1 }

# 4. HelloAtom as this project (Getting-Started section 7).
$main = (Get-Content (Join-Path $hello "Main.cpp") -Raw).Replace('m_root + "Assets/HelloAtom/"', 'm_root + ATOM_PROJECT_ASSETS')
if (-not $main.Contains("ATOM_PROJECT_ASSETS")) { Write-Host "FAILED: HelloAtom's asset folder line moved - update this test and Getting-Started" -ForegroundColor Red; exit 1 }
Set-Content -LiteralPath (Join-Path $game "Source/Main.cpp") -Value $main -NoNewline
Copy-Item (Join-Path $hello "Assets/Levels/hello.json") (Join-Path $game "Assets/Levels/hello.json")
$cmakeLists = Join-Path $game "CMakeLists.txt"
Set-Content -LiteralPath $cmakeLists -Value (Get-Content $cmakeLists -Raw).Replace("atom_add_project()", "atom_add_project(CONTENT First Kit)") -NoNewline

# 5. Its own build, through its preset; each configuration built and run.
Step "configure MyGame (cmake --preset atom)" { & $cmake -E chdir $game $cmake --preset atom }
foreach ($config in $Configs) {
    Step "build MyGame ($config)" { & $cmake --build (Join-Path $game "build") --config $config --target MyGame --parallel }
    $folder = Join-Path $game "build/bin/$config/MyGame"
    $env:ATOM_TEST_SCRIPT = Join-Path $hello "Scenarios/hello_atom.atomtest"
    try {
        $out = Join-Path $work "run-$config.txt"
        $process = Start-Process (Join-Path $folder "MyGame.exe") -WorkingDirectory $folder -RedirectStandardOutput $out -PassThru -Wait
    }
    finally { Remove-Item Env:ATOM_TEST_SCRIPT }
    if ($process.ExitCode -ne 0 -or -not (Select-String -Path $out -Pattern "hello atom done" -Quiet)) {
        Get-Content $out | Select-Object -Last 20 | ForEach-Object { Write-Host "   $_" }
        Write-Host "FAILED: MyGame ($config) didn't pass HelloAtom's scenario (exit $($process.ExitCode))" -ForegroundColor Red
        exit 1
    }
    Write-Host "   ok: MyGame ($config) ran HelloAtom's scenario from its own folder"
}

Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
Write-Host ("PASS: a game built from {0}, outside the repository ({1}) in {2:N0} s" -f (Split-Path $Zip -Leaf), ($Configs -join ", "),
    ((Get-Date) - $started).TotalSeconds)
