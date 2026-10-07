# The validation ladder: check a change with the cheapest step that can
# catch its mistakes, and climb only when that passes.
#
#   pwsh Tools/Dev/check.ps1 -Level docs
#   pwsh Tools/Dev/check.ps1 -Level quick   [-Config Debug|Release]
#   pwsh Tools/Dev/check.ps1 -Level feature -Scenario lakeshore,environment [-Config ...]
#   pwsh Tools/Dev/check.ps1 -Level changed [-Base origin/master] [-DryRun]
#   pwsh Tools/Dev/check.ps1 -Level full
#
#   docs     .md, CHANGELOG, comments in docs, .gitignore, the CI workflow:
#            nothing to build (CI tests its own workflow on push).
#   quick    an incremental build of one configuration, then the unit and
#            authoring tests (seconds). Content JSON (levels, presets): this.
#   feature  quick, then the named in-game scenarios (they need a GPU).
#   changed  (M81) what the branch touched since it left -Base, committed or
#            not: each changed file is mapped to scenarios by
#            Tools/Dev/changed.psd1 (printed, with the reason), then quick
#            plus those scenarios - and a package check when packaging or
#            CMake changed. Docs-only: nothing. Unmapped files get a broad
#            fallback set, loudly. -DryRun prints the plan only. The
#            per-milestone check.
#   full     Debug and Release, every test and scenario in both: once per
#            version, before the release commit.
#
# Pick the configuration where the defect shows: Debug for asserts and
# lifetime checks, Release for anything timed. A shader-only change still
# uses quick/feature: the build compiles just the shaders that changed.
# Assets are never rebuilt here - only when Blender scripts or content
# products change (Tools/Blender/build_assets.py).
param(
    [Parameter(Mandatory)] [ValidateSet("docs", "quick", "feature", "changed", "full")] [string]$Level,
    [ValidateSet("Debug", "Release")] [string]$Config = "Debug",
    [string[]]$Scenario = @(),
    [string]$BuildDir = "build",
    [string]$Base = "origin/master", # -Level changed: what the branch is compared with
    [switch]$DryRun                  # -Level changed: print the plan, run nothing
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
Set-Location $root

if ($Level -eq "docs") {
    Write-Host "docs: nothing to build or test."
    exit 0
}

$packages = @()
if ($Level -eq "changed") {
    # The files: since the branch left $Base (committed or not), plus untracked ones.
    $mergeBase = git merge-base HEAD $Base 2>$null
    if (-not $mergeBase) { $mergeBase = git merge-base HEAD master 2>$null }
    if (-not $mergeBase) { throw "No merge base with $Base (or master): pass -Base <ref>" }
    $files = @(@(git diff --name-only $mergeBase) + @(git ls-files --others --exclude-standard) |
        Where-Object { $_ } | Sort-Object -Unique)
    $table = Import-PowerShellDataFile (Join-Path $PSScriptRoot "changed.psd1")

    $chosen = [System.Collections.Generic.SortedSet[string]]::new()
    $needsBuild = $false
    $unmapped = @()
    Write-Host ("changed: {0} file(s) since {1} ({2})" -f $files.Count, $Base, $mergeBase.Substring(0, 7))
    foreach ($file in $files) {
        $rule = $table.Rules | Where-Object { $file -match $_.Pattern } | Select-Object -First 1
        if (-not $rule) {
            $unmapped += $file
            $needsBuild = $true
            foreach ($s in $table.Fallback) { [void]$chosen.Add($s) }
            Write-Host ("  {0}  ->  UNMAPPED: fallback {1}" -f $file, ($table.Fallback -join ", ")) -ForegroundColor Yellow
            continue
        }
        # $file -match ran last on the matching rule: $Matches holds its groups.
        [void]($file -match $rule.Pattern)
        $names = @($rule.Scenarios | ForEach-Object { if ($_ -eq '$scenario') { $Matches['scenario'] } else { $_ } })
        if ($rule.Build -ne $false) { $needsBuild = $true }
        foreach ($s in $names) { [void]$chosen.Add($s) }
        if ($rule.Package -and $packages -notcontains $rule.Package) { $packages += $rule.Package }
        $what = if ($rule.Build -eq $false) { "nothing to build" }
                elseif ($names.Count) { $names -join ", " } else { "unit tests" }
        if ($rule.Package) { $what += "; package $($rule.Package)" }
        Write-Host ("  {0}  ->  {1}" -f $file, $what)
    }
    if ($unmapped.Count) {
        Write-Host ("{0} unmapped file(s): add a rule to Tools/Dev/changed.psd1" -f $unmapped.Count) -ForegroundColor Yellow
    }
    if (-not $needsBuild) {
        Write-Host "changed: docs only - nothing to build or test."
        exit 0
    }
    $Scenario = @($chosen)
    Write-Host ("plan: build {0}, unit tests{1}{2}" -f $Config,
        $(if ($Scenario.Count) { ", scenarios " + ($Scenario -join ", ") } else { "" }),
        $(if ($packages.Count) { ", package check " + ($packages -join ", ") } else { "" }))
    if ($DryRun) { exit 0 }
}
# "-Scenario a,b" arrives as one string through `pwsh -File`: split it.
$Scenario = @($Scenario | ForEach-Object { $_ -split "," } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($Level -eq "feature" -and $Scenario.Count -eq 0) {
    throw "-Level feature needs -Scenario <name>[,<name>...] (Tests/Scenarios/<name>.atomtest)"
}

# cmake and ctest: on PATH, or the copies inside Visual Studio (common.ps1).
. "$PSScriptRoot/common.ps1"
$cmake = Find-DevTool "cmake"
$ctest = Find-DevTool "ctest"
if (-not $cmake -or -not $ctest) { throw "cmake/ctest not found (install CMake or Visual Studio's CMake component); run Tools/Dev/doctor.ps1" }

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
    if (($Level -eq "feature" -or $Level -eq "changed") -and $Scenario.Count) {
        # Every name must exist: a misspelt one (or a stale table entry) fails
        # instead of quietly running fewer scenarios.
        $known = @(& $ctest --test-dir $BuildDir -C $c -N | ForEach-Object { if ($_ -match 'Scenario\.(\S+)') { $Matches[1] } })
        $missing = @($Scenario | Where-Object { $known -notcontains $_ })
        if ($missing.Count) {
            Write-Host "FAILED: no such scenario: $($missing -join ', ')" -ForegroundColor Red
            exit 1
        }
        $pattern = "^Scenario\.(" + ($Scenario -join "|") + ")$"
        Step "scenarios $($Scenario -join ', '), $c" { & $ctest --test-dir $BuildDir -C $c -R $pattern --no-tests=error --output-on-failure }
    }
}
foreach ($game in $packages) {
    # M81: packaging or CMake changed - stage and verify that game's package.
    Step "package check $game" { pwsh -NoProfile -File (Join-Path $PSScriptRoot "../Dist/package.ps1") -Game $game -NoSmoke }
}
Write-Host "PASS ($Level)"
