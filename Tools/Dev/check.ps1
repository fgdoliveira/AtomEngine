# The validation ladder: check a change with the cheapest step that can
# catch its mistakes, and climb only when that passes.
#
#   pwsh Tools/Dev/check.ps1 -Level docs
#   pwsh Tools/Dev/check.ps1 -Level quick   [-Config Debug|Release]
#   pwsh Tools/Dev/check.ps1 -Level feature -Scenario showcase_tour,demo.environment [-Config ...]
#   pwsh Tools/Dev/check.ps1 -Level changed [-Base origin/master] [-DryRun]
#   pwsh Tools/Dev/check.ps1 -Level full
#
#   docs     .md, CHANGELOG, comments in docs, .gitignore, the CI workflow:
#            nothing to build (CI tests its own workflow on push).
#   quick    an incremental build of one configuration, then every CPU test
#            - each owner's unit and content checks (seconds).
#   feature  quick, then the named in-game scenarios (they need a GPU). A
#            name is <owner>.<name> or, when only one owner has it, <name>.
#   changed  (M81; by owner since M107) what the branch touched since it
#            left -Base, committed or not: each changed file is mapped by
#            Tools/Dev/changed.psd1 (printed, with the reason) to the owners
#            whose CPU tests to run, the scenarios, and a package check when
#            packaging or CMake changed. Only those test targets and apps
#            are built: a demo change compiles no engine test. Docs-only:
#            nothing. Unmapped files get a broad fallback, loudly. -DryRun
#            prints the plan only. The per-milestone check.
#   full     Debug and Release, everything built, every test and scenario
#            in both: once per version, before the release commit.
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

$table = Import-PowerShellDataFile (Join-Path $PSScriptRoot "changed.psd1")
$owners = [System.Collections.Generic.SortedSet[string]]::new() # CPU tests (-Level changed)
$smoke = $false
$packages = @()
if ($Level -eq "changed") {
    # The files: since the branch left $Base (committed or not), plus untracked ones.
    $mergeBase = git merge-base HEAD $Base 2>$null
    if (-not $mergeBase) { $mergeBase = git merge-base HEAD master 2>$null }
    if (-not $mergeBase) { throw "No merge base with $Base (or master): pass -Base <ref>" }
    $files = @(@(git diff --name-only $mergeBase) + @(git ls-files --others --exclude-standard) |
        Where-Object { $_ } | Sort-Object -Unique)

    $chosen = [System.Collections.Generic.SortedSet[string]]::new()
    $needsBuild = $false
    $unmapped = @()
    Write-Host ("changed: {0} file(s) since {1} ({2})" -f $files.Count, $Base, $mergeBase.Substring(0, 7))
    foreach ($file in $files) {
        $rule = $table.Rules | Where-Object { $file -match $_.Pattern } | Select-Object -First 1
        if (-not $rule) {
            $unmapped += $file
            $rule = $table.Fallback
            Write-Host ("  {0}  ->  UNMAPPED: fallback" -f $file) -ForegroundColor Yellow
        }
        else {
            # $file -match ran last on the matching rule: $Matches holds its groups.
            [void]($file -match $rule.Pattern)
        }
        $names = @($rule.Scenarios | Where-Object { $_ } | ForEach-Object { $_ -replace '\$scenario', $Matches['scenario'] })
        # A scenario file the branch deleted runs nothing (its test is gone).
        if (($rule.Scenarios -match '\$scenario') -and -not (Test-Path -LiteralPath $file)) { $names = @() }
        if ($rule.Build -ne $false) { $needsBuild = $true }
        foreach ($o in @($rule.Tests | Where-Object { $_ })) { [void]$owners.Add($o) }
        foreach ($s in $names) { [void]$chosen.Add($s) }
        if ($rule.Smoke) { $smoke = $true }
        if ($rule.Package -and $packages -notcontains $rule.Package) { $packages += $rule.Package }
        $what = @()
        if ($rule.Build -eq $false) { $what += "nothing to build" }
        if ($rule.Tests) { $what += "tests " + ($rule.Tests -join ", ") }
        if ($rule.Smoke) { $what += "smoke" }
        if ($names.Count) { $what += $names -join ", " }
        if ($rule.Package) { $what += "package $($rule.Package)" }
        if (-not $what.Count) { $what += "build only" }
        Write-Host ("  {0}  ->  {1}" -f $file, ($what -join "; "))
    }
    if ($unmapped.Count) {
        Write-Host ("{0} unmapped file(s): add a rule to Tools/Dev/changed.psd1" -f $unmapped.Count) -ForegroundColor Yellow
    }
    if (-not $needsBuild -and -not $packages.Count) {
        Write-Host "changed: nothing to build - docs or scripts only."
        exit 0
    }
    $Scenario = @($chosen)
    Write-Host ("plan: build {0}: CPU tests {1}{2}{3}{4}" -f $Config,
        $(if ($owners.Count) { $owners -join ", " } else { "none" }),
        $(if ($smoke) { ", every app's smoke scenario" } else { "" }),
        $(if ($Scenario.Count) { ", scenarios " + ($Scenario -join ", ") } else { "" }),
        $(if ($packages.Count) { ", package check " + ($packages -join ", ") } else { "" }))
    if ($DryRun) { exit 0 }
}
# "-Scenario a,b" arrives as one string through `pwsh -File`: split it.
$Scenario = @($Scenario | ForEach-Object { $_ -split "," } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($Level -eq "feature" -and $Scenario.Count -eq 0) {
    throw "-Level feature needs -Scenario <name>[,<name>...] (an app's Scenarios/<name>.atomtest)"
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

# Scenario.<owner>.<name> for each name: as given when qualified, else the
# one owner that has it. Unknown or ambiguous names fail - a misspelt one
# (or a stale table entry) must not quietly run fewer scenarios.
function Resolve-Scenarios([string[]]$names, [string]$config) {
    $known = @(& $ctest --test-dir $BuildDir -C $config -N | ForEach-Object { if ($_ -match 'Scenario\.(\S+)') { $Matches[1] } })
    $resolved = @()
    foreach ($name in $names) {
        $found = @(if ($name.Contains(".")) { $known | Where-Object { $_ -eq $name } }
                   else { $known | Where-Object { $_.EndsWith(".$name") } })
        if ($found.Count -ne 1) {
            $why = if ($found.Count) { "ambiguous, say which: " + ($found -join ", ") } else { "no such scenario" }
            Write-Host "FAILED: ${name}: $why" -ForegroundColor Red
            exit 1
        }
        $resolved += $found[0]
    }
    return $resolved
}

# Configured once; after that every build re-runs CMake itself when a
# CMakeLists changed, so a new test or scenario is registered before ctest
# is asked for it (the build comes first below).
if (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
    Step "configure" { & $cmake -B $BuildDir -S . }
}

$configs = if ($Level -eq "full") { @("Debug", "Release") } else { @($Config) }
foreach ($c in $configs) {
    if ($Level -ne "changed") {
        Step "build $c" { & $cmake --build $BuildDir --config $c }
        if ($Level -eq "full") {
            Step "all tests and scenarios, $c" { & $ctest --test-dir $BuildDir -C $c --output-on-failure }
            continue
        }
        Step "CPU tests, $c" { & $ctest --test-dir $BuildDir -C $c -L cpu --output-on-failure }
        if ($Level -eq "feature") {
            $names = Resolve-Scenarios $Scenario $c
            $pattern = "^Scenario\.(" + (($names | ForEach-Object { [regex]::Escape($_) }) -join "|") + ")$"
            Step "scenarios $($names -join ', '), $c" { & $ctest --test-dir $BuildDir -C $c -R $pattern --no-tests=error --output-on-failure }
        }
        continue
    }

    # -Level changed: only the owners' test targets and the apps the
    # scenarios run (the table's names are <owner>.<name>; smoke is every app).
    $apps = if ($smoke) { @($table.Apps.Values) } else { @($Scenario | ForEach-Object { $table.Apps[$_.Split(".")[0]] }) }
    # M112: every owner's CPU tests include its levels' check (LevelCheck.<owner>).
    $tools = if ($owners.Count) { @("AtomLevelCheck") } else { @() }
    $targets = @(@($owners | ForEach-Object { $table.TestTargets[$_] }) + $apps + $tools | Where-Object { $_ } | Sort-Object -Unique)
    if ($smoke) {
        # Every app relinks anyway: one parallel build of everything beats
        # the Visual Studio generator's one-MSBuild-per---target, in turn.
        Step "build $c (every app: the smoke runs)" { & $cmake --build $BuildDir --config $c }
    }
    elseif ($targets.Count) {
        Step "build $c`: $($targets -join ', ')" { & $cmake --build $BuildDir --config $c --target @targets }
    }
    $names = @()
    if ($Scenario.Count) { $names += Resolve-Scenarios $Scenario $c }
    if ($smoke) {
        $names += @(& $ctest --test-dir $BuildDir -C $c -N -L smoke | ForEach-Object { if ($_ -match 'Scenario\.(\S+)') { $Matches[1] } })
    }
    $names = @($names | Sort-Object -Unique)
    if ($owners.Count) {
        $pattern = "^(Unit|Content|LevelCheck)\.(" + ($owners -join "|") + ")$"
        Step "CPU tests $($owners -join ', '), $c" { & $ctest --test-dir $BuildDir -C $c -R $pattern --no-tests=error --output-on-failure }
    }
    if ($names.Count) {
        $pattern = "^Scenario\.(" + (($names | ForEach-Object { [regex]::Escape($_) }) -join "|") + ")$"
        Step "scenarios $($names -join ', '), $c" { & $ctest --test-dir $BuildDir -C $c -R $pattern --no-tests=error --output-on-failure }
    }
}
foreach ($game in $packages) {
    # M81: packaging or CMake changed - stage and verify that game's package.
    Step "package check $game" { pwsh -NoProfile -File (Join-Path $PSScriptRoot "../Dist/package.ps1") -Game $game -NoSmoke }
}
Write-Host "PASS ($Level)"
