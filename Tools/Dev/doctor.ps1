# Can this machine build and run AtomEngine? (M62) A convenience, never a
# requirement: the game runs without it.
#
#   pwsh Tools/Dev/doctor.ps1                       # prerequisites
#   pwsh Tools/Dev/doctor.ps1 -Configure            # ... and a test configure
#   pwsh Tools/Dev/doctor.ps1 -GamePath build/bin/Release/AtomGame.exe   # ... and the game's own report
#
# Each check prints PASS, WARN or FAIL with what to do. It reads, and never
# changes anything: no power plans, drivers, environment variables or
# repository files (a -Configure goes to a throwaway folder).
param(
    [switch]$Configure,
    [string]$GamePath = ""
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot/common.ps1"
$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
$failures = 0
$warnings = 0

function Report([string]$status, [string]$what, [string]$detail) {
    $color = @{ PASS = "Green"; WARN = "Yellow"; FAIL = "Red" }[$status]
    Write-Host ("[{0}] {1}" -f $status, $what) -ForegroundColor $color -NoNewline
    Write-Host ("  {0}" -f $detail)
    if ($status -eq "FAIL") { $script:failures++ }
    if ($status -eq "WARN") { $script:warnings++ }
}

# Windows 10 or 11 (build 10240 onward).
$os = [System.Environment]::OSVersion.Version
if ($IsWindows -or $env:OS -eq "Windows_NT") {
    if ($os.Major -ge 10) { Report PASS "Windows" ("{0} (build {1})" -f $os, $os.Build) }
    else { Report FAIL "Windows" "Windows 10 or 11 is required" }
}
else { Report FAIL "Windows" "AtomEngine builds for Windows (Direct3D 12) only" }

# CMake >= 3.26.
$cmake = Find-DevTool "cmake"
if (-not $cmake) { Report FAIL "CMake" "not found: install CMake 3.26+ or Visual Studio's CMake component" }
else {
    $version = [version](((& $cmake --version) | Select-Object -First 1) -replace "[^0-9.]", "")
    if ($version -ge [version]"3.26") { Report PASS "CMake" "$version ($cmake)" }
    else { Report FAIL "CMake" "$version is too old: 3.26 or later is required" }
}

# Visual Studio with the C++ tools.
$vs = Get-VisualStudioPath
if ($vs) { Report PASS "Visual Studio C++" $vs }
else { Report FAIL "Visual Studio C++" "install Visual Studio 2022+ with 'Desktop development with C++'" }

# dxc, for the shaders (not needed for -DATOM_BUILD_GAME=OFF).
$dxc = Find-Dxc
if ($dxc) { Report PASS "dxc (shader compiler)" $dxc }
else { Report FAIL "dxc (shader compiler)" "install the Windows SDK (or the Vulkan SDK); only -DATOM_BUILD_GAME=OFF builds without it" }

# Submodules checked out.
$missing = @("SDL", "glm", "cgltf", "stb", "json", "doctest", "imgui") |
    Where-Object { -not (Test-Path (Join-Path $root "external/$_/*")) }
if ($missing.Count -eq 0) { Report PASS "Submodules" "all present" }
else { Report FAIL "Submodules" ("missing: {0} - run: git submodule update --init --recursive" -f ($missing -join ", ")) }

# The runtime asset payload (Game/CMakeLists.txt's list) in the source tree.
$listText = Get-Content -Raw (Join-Path $root "Game/CMakeLists.txt")
if ($listText -match "set\(ATOM_RUNTIME_ASSETS([^)]*)\)") {
    $folders = $Matches[1] -split "\s+" | Where-Object { $_ }
    $absent = $folders | Where-Object { -not (Test-Path (Join-Path $root "Assets/$_")) }
    if ($absent.Count -eq 0) { Report PASS "Runtime assets" ("{0} folders present" -f $folders.Count) }
    else { Report FAIL "Runtime assets" ("missing under Assets/: {0}" -f ($absent -join ", ")) }
}
else { Report WARN "Runtime assets" "could not read the payload list in Game/CMakeLists.txt" }

# Power: performance numbers on battery mean little.
$power = Get-PowerSource
if ($power -eq "battery") { Report WARN "Power" "on battery: build and play fine, but don't measure performance" }
else { Report PASS "Power" $power }

# A configure in a throwaway folder, if asked.
if ($Configure -and $cmake) {
    $scratch = Join-Path ([System.IO.Path]::GetTempPath()) ("atom_doctor_" + [guid]::NewGuid().ToString("N").Substring(0, 8))
    try {
        $output = & $cmake -S $root -B $scratch 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0) { Report PASS "Configure" "a fresh configure succeeded" }
        else { Report FAIL "Configure" ("failed; last lines:`n{0}" -f (($output -split "`n" | Select-Object -Last 8) -join "`n")) }
    }
    finally { Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $scratch }
}

# The game's own view of the machine.
if ($GamePath) {
    if (-not (Test-Path $GamePath)) { Report FAIL "Game diagnostics" "no game at $GamePath (build it first)" }
    else {
        $report = Join-Path ([System.IO.Path]::GetTempPath()) "atom_diagnostics.txt"
        $process = Start-Process -FilePath $GamePath -ArgumentList "--no-settings", "--diagnostics", "`"$report`"" -PassThru -NoNewWindow `
            -RedirectStandardOutput (Join-Path ([System.IO.Path]::GetTempPath()) "atom_doctor_out.txt")
        if (-not $process.WaitForExit(60000)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            Report FAIL "Game diagnostics" "the game didn't finish within 60 s (killed)"
        }
        elseif ($process.ExitCode -ne 0 -or -not (Test-Path $report)) {
            Report FAIL "Game diagnostics" ("the game exited with {0} and no report" -f $process.ExitCode)
        }
        else {
            Report PASS "Game diagnostics" "the engine started on this machine:"
            Get-Content $report | ForEach-Object { Write-Host "        $_" }
            $lines = Get-Content -Raw $report
            if ($lines -match "display\.mode: .* @ (\d+) Hz" -and [int]$Matches[1] -le 60) {
                Report WARN "Display" "the window is on a $($Matches[1]) Hz display: frame-time measurements may be capped there; measure on the laptop's own screen"
            }
            Remove-Item -ErrorAction SilentlyContinue $report
        }
    }
}

Write-Host ""
Write-Host ("{0} failed, {1} warning(s)." -f $failures, $warnings)
if ($failures -gt 0) { exit 1 }
