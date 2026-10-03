# The validation ladder: check a change with the cheapest step that can
# catch its mistakes, and climb only when that passes.
#
#   pwsh Tools/Dev/check.ps1 -Level docs
#   pwsh Tools/Dev/check.ps1 -Level quick   [-Config Debug|Release]
#   pwsh Tools/Dev/check.ps1 -Level feature -Scenario lakeshore,environment [-Config ...]
#   pwsh Tools/Dev/check.ps1 -Level full
#
#   docs     .md, CHANGELOG, comments in docs, .gitignore, the CI workflow:
#            nothing to build (CI tests its own workflow on push).
#   quick    an incremental build of one configuration, then the unit and
#            authoring tests (seconds). Content JSON (levels, presets): this.
#   feature  quick, then the named in-game scenarios (they need a GPU).
#   full     Debug and Release, every test and scenario in both: the gate
#            before a milestone or release commit.
#
# Pick the configuration where the defect shows: Debug for asserts and
# lifetime checks, Release for anything timed. A shader-only change still
# uses quick/feature: the build compiles just the shaders that changed.
# Assets are never rebuilt here - only when Blender scripts or content
# products change (Tools/Blender/build_assets.py).
param(
    [Parameter(Mandatory)] [ValidateSet("docs", "quick", "feature", "full")] [string]$Level,
    [ValidateSet("Debug", "Release")] [string]$Config = "Debug",
    [string[]]$Scenario = @(),
    [string]$BuildDir = "build"
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
Set-Location $root

if ($Level -eq "docs") {
    Write-Host "docs: nothing to build or test."
    exit 0
}
if ($Level -eq "feature" -and $Scenario.Count -eq 0) {
    throw "-Level feature needs -Scenario <name>[,<name>...] (Tests/Scenarios/<name>.atomtest)"
}

# cmake and ctest: on PATH, or the copies inside Visual Studio.
function Find-Tool([string]$name) {
    $onPath = Get-Command $name -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -property installationPath
        $candidate = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\$name.exe"
        if (Test-Path $candidate) { return $candidate }
    }
    throw "$name not found (install CMake or Visual Studio's CMake component)"
}
$cmake = Find-Tool "cmake"
$ctest = Find-Tool "ctest"

function Step([string]$what, [scriptblock]$action) {
    Write-Host "== $what"
    $started = Get-Date
    & $action
    if ($LASTEXITCODE -ne 0) {
        Write-Host ("FAILED: {0} (exit {1})" -f $what, $LASTEXITCODE)
        exit 1
    }
    Write-Host ("   ok ({0:N0} s)" -f ((Get-Date) - $started).TotalSeconds)
}

if (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
    Step "configure" { & $cmake -B $BuildDir -S . }
}

$configs = if ($Level -eq "full") { @("Debug", "Release") } else { @($Config) }
foreach ($c in $configs) {
    Step "build $c" { & $cmake --build $BuildDir --config $c }
    if ($Level -eq "full") {
        Step "all tests and scenarios, $c" { & $ctest --test-dir $BuildDir -C $c --output-on-failure }
        continue
    }
    Step "unit and authoring tests, $c" { & $ctest --test-dir $BuildDir -C $c -LE scenario --output-on-failure }
    if ($Level -eq "feature") {
        $pattern = "^Scenario\.(" + ($Scenario -join "|") + ")$"
        Step "scenarios $($Scenario -join ', '), $c" { & $ctest --test-dir $BuildDir -C $c -R $pattern --output-on-failure }
    }
}
Write-Host "PASS ($Level)"
